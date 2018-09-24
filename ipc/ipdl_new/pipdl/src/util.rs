/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Fairly minimal recursive-descent parser helper functions and types.

use std::fmt;

#[derive(Clone, Debug)]
pub struct Location {
    pub line: usize,
    pub col: usize,
    pub file: String,
}

impl Location {
    fn new(file: String) -> Self {
        Location {
            line: 0,
            col: 0,
            file,
        }
    }

    pub fn is_null(&self) -> bool {
        self.line == 0
    }
}

#[derive(Clone, Debug)]
pub struct Span {
    pub start: Location,
    pub end: Location,
}

impl Span {
    pub fn new(file: String) -> Self {
        Span {
            start: Location::new(file.clone()),
            end: Location::new(file),
        }
    }

    pub fn is_null(&self) -> bool {
        self.start.is_null() && self.end.is_null()
    }
}

#[derive(Clone, Debug)]
pub struct Spanned<T> {
    pub data: T,
    pub span: Span,
}

impl<T> Spanned<T> {
    pub fn new(span: Span, data: T) -> Self {
        Spanned { data, span }
    }
}

/// Every bit set but the high bit in usize.
const OFF_MASK: usize = <usize>::max_value() / 2;

/// Only the high bit in usize.
const FATAL_MASK: usize = !OFF_MASK;

/// An error produced by pipdl
pub struct ParserError {
    message: String,
    fatal: bool,
    span: Span,
}

impl ParserError {
    pub(crate) fn is_fatal(&self) -> bool {
        self.fatal
    }

    pub(crate) fn make_fatal(mut self) -> Self {
        self.fatal = true;
        self
    }

    pub(crate) fn span(&self) -> Span {
        self.span.clone()
    }

    pub(crate) fn message(&self) -> &str {
        &self.message
    }
}

impl fmt::Debug for ParserError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Error")
            .field("file", &self.span.start.file)
            .field("start_line", &self.span.start.line)
            .field("start_column", &self.span.start.col)
            .field("end_line", &self.span.end.line)
            .field("end_column", &self.span.end.col)
            .finish()
    }
}

/// Attempts to run each expression in order, recovering from any non-fatal
/// errors and attempting the next option.
macro_rules! any {
    ($i:ident, $expected:expr, $($e:expr => |$x:ident| $f:expr),+ $(,)*) => {
        // NOTE: We have to do this sorcery for early returns. Using a loop with breaks makes clippy
        // mad because the loop never loops and we can't desable the lint because we're not in a function.
        // Also, we don't directly call the closure otherwise clippy would also complain about that. Yeah.
        {
            let mut my_closure = || {
                $(match $e {
                    Ok((i, $x)) => return Ok((i, $f)),
                    Err(e) => {
                        // This assignment is used to help out with type inference.
                        let e: $crate::util::ParserError = e;
                        if e.is_fatal() {
                            return Err(e);
                        }
                    }
                })+
                return $i.expected($expected);
            };

            my_closure()
        }
    };

    ($i:ident, $expected:expr, $($e:expr => $f:expr),+ $(,)*) => {
        any!($i, $expected, $($e => |_x| $f),+);
    }
}

/// Attempts to repeatedly run the expression, stopping on a non-fatal error,
/// and directly returning any fatal error.
macro_rules! drive {
    ($i:ident, $e:expr) => {
        let mut $i = $i;
        loop {
            match $e {
                Ok((j, _)) => $i = j,
                Err(e) => if e.is_fatal() {
                    return Err(e);
                } else {
                    break;
                },
            }
        }
    };
}

/// The type of error used by internal parsers
pub(crate) type PResult<'src, T> = Result<(In<'src>, T), ParserError>;

/// Specify that after this point, errors produced while parsing which are not
/// handled should instead be treated as fatal parsing errors.
macro_rules! commit {
    ($($e:tt)*) => {
        // Evaluate the inner expression, transforming errors into fatal errors.
        let eval = || { $($e)* };
        eval().map_err($crate::util::ParserError::make_fatal)
    }
}

/// This datastructure is used as the cursor type into the input source data. It
/// holds the full source string, and the current offset.
#[derive(Clone, Debug)]
pub(crate) struct In<'src> {
    src: &'src str,
    byte_offset: usize,
    loc: Location,
}
impl<'src> In<'src> {
    pub(crate) fn new(s: &'src str, file: String) -> Self {
        In {
            src: s,
            byte_offset: 0,
            loc: Location {
                line: 1,
                col: 0,
                file,
            },
        }
    }

    /// The remaining string in the source file.
    pub(crate) fn rest(&self) -> &'src str {
        &self.src[self.byte_offset..]
    }

    /// Move the cursor forward by `n` characters.
    pub(crate) fn advance(&self, n_chars: usize) -> Self {
        let mut loc = self.loc.clone();

        let (n_bytes, last_c) = self
            .rest()
            .char_indices()
            .take(n_chars)
            .inspect(|&(_, character)| {
                if character == '\n' {
                    loc.line += 1;
                    loc.col = 0;
                } else {
                    loc.col += 1;
                }
            })
            .last()
            .expect("No characters remaining in advance");

        let byte_offset = self
            .byte_offset
            .checked_add(n_bytes + last_c.len_utf8())
            .expect("Failed checked add");

        assert!(byte_offset <= self.src.len());

        In {
            src: self.src,
            byte_offset,
            loc,
        }
    }

    /// Produce a new non-fatal error result with the given expected value.
    pub(crate) fn expected<T>(&self, expected: &'static str) -> Result<T, ParserError> {
        assert!((self.byte_offset & FATAL_MASK) == 0, "Offset is too large!");

        // Get the line where the error occurred.
        let text = self.src.lines().nth(self.loc.line - 1).unwrap_or(""); // Usually happens when the error occurs on the last, empty line

        // Format the final error message.
        let message = format!(
            "{}\n\
             | {}\n{:~>off$}^\n",
            format!("Expected {}", expected),
            text,
            "",
            off = self.loc.col + 2
        );

        Err(ParserError {
            message,
            fatal: false,
            span: Span {
                start: self.loc.clone(),
                end: self.loc.clone(),
            },
        })
    }

    pub(crate) fn loc(&self) -> Location {
        self.loc.clone()
    }
}

/// Repeatedly run f, collecting results into a vec. Returns an error if a fatal
/// error is produced while parsing.
pub(crate) fn many<'src, F, R>(i: In<'src>, mut f: F) -> PResult<'src, Vec<R>>
where
    F: FnMut(In<'src>) -> PResult<'src, R>,
{
    let mut v = Vec::new();
    drive!(
        i,
        match f(i.clone()) {
            Ok((i, x)) => {
                v.push(x);
                Ok((i, ()))
            }
            Err(e) => Err(e),
        }
    );
    Ok((i, v))
}

/// Repeatedly run f, followed by parsing the seperator sep. Returns an error if
/// a fatal error is produced while parsing.
pub(crate) fn sep<'src, Parser, Ret>(
    i: In<'src>,
    mut parser: Parser,
    sep: &'static str,
) -> PResult<'src, Vec<Ret>>
where
    Parser: FnMut(In<'src>) -> PResult<'src, Ret>,
{
    let mut return_vector = Vec::new();
    drive!(
        i,
        match parser(i.clone()) {
            Ok((i, result)) => {
                return_vector.push(result);
                match punct(i.clone(), sep) {
                    Ok(o) => Ok(o),
                    Err(_) => return Ok((i, return_vector)),
                }
            }
            Err(e) => Err(e),
        }
    );
    Ok((i, return_vector))
}

/// Skip any leading whitespace, including comments
pub(crate) fn skip_ws(mut i: In) -> Result<In, ParserError> {
    loop {
        if i.rest().is_empty() {
            break;
        }

        let c = i
            .rest()
            .chars()
            .next()
            .expect("No characters remaining when skipping ws");
        if c.is_whitespace() {
            i = i.advance(1);
            continue;
        }

        // Line comments
        if i.rest().starts_with("//") {
            while !i.rest().starts_with('\n') {
                i = i.advance(1);
                if i.rest().is_empty() {
                    break;
                }
            }
            continue;
        }

        // Block comments
        if i.rest().starts_with("/*") {
            while !i.rest().starts_with("*/") {
                i = i.advance(1);
                if i.rest().is_empty() {
                    return i.expected("end of block comment (`*/`)");
                }
            }

            i = i.advance(2);
            continue;
        }
        break;
    }

    Ok(i)
}

/// Read an identifier as a string.
pub(crate) fn ident(i: In) -> PResult<Spanned<String>> {
    let i = skip_ws(i)?;
    let start = i.loc();
    let (end_char, end_byte) = i
        .rest()
        .char_indices()
        .enumerate()
        .skip_while(|&(_, (b_idx, c))| match c {
            '_' | 'a'...'z' | 'A'...'Z' => true,
            '0'...'9' if b_idx != 0 => true,
            _ => false,
        })
        .next()
        .map(|(c_idx, (b_idx, _))| (c_idx, b_idx))
        .unwrap_or((i.rest().chars().count(), i.rest().len()));

    if end_byte == 0 {
        return i.expected("identifier");
    }

    let j = i.advance(end_char);
    let end = j.loc();

    Ok((
        j,
        Spanned::new(Span { start, end }, i.rest()[..end_byte].to_owned()),
    ))
}

/// Parse a specific keyword.
pub(crate) fn kw<'src>(i: In<'src>, kw: &'static str) -> PResult<'src, Spanned<()>> {
    let error_message = i.expected(kw);

    let (i, id) = ident(i)?;
    if id.data == kw {
        Ok((i, Spanned::new(id.span, ())))
    } else {
        error_message
    }
}

/// Parse punctuation.
pub(crate) fn punct<'src>(i: In<'src>, p: &'static str) -> PResult<'src, Spanned<()>> {
    let i = skip_ws(i)?;
    let start = i.loc();
    if i.rest().starts_with(p) {
        let i = i.advance(p.chars().count());
        let end = i.loc();
        Ok((i, Spanned::new(Span { start, end }, ())))
    } else {
        i.expected(p)
    }
}

/// Try to parse the inner value, and return Some() if it succeeded, None if it
/// failed non-fatally, and an error if it failed fatally.
pub(crate) fn maybe<'src, T>(i: In<'src>, r: PResult<'src, T>) -> PResult<'src, Option<T>> {
    match r {
        Ok((i, x)) => Ok((i, Some(x))),
        Err(e) => if e.is_fatal() {
            Err(e)
        } else {
            Ok((i, None))
        },
    }
}

/// Parse a string literal.
pub(crate) fn string(i: In) -> PResult<Spanned<String>> {
    let mut s = String::new();
    let start = i.loc();
    let (i, _) = punct(i, "\"")?;
    let mut chars = i.rest().chars().enumerate().peekable();
    while let Some((char_offset, ch)) = chars.next() {
        match ch {
            '"' => {
                let i = i.advance(char_offset + 1);
                let end = i.loc();
                return Ok((i, Spanned::new(Span { start, end }, s)));
            }
            '\\' => match chars.next() {
                Some((_, 'n')) => s.push('\n'),
                Some((_, 'r')) => s.push('\r'),
                Some((_, 't')) => s.push('\t'),
                Some((_, '\\')) => s.push('\\'),
                Some((_, '\'')) => s.push('\''),
                Some((_, '"')) => s.push('"'),
                Some((_, '0')) => s.push('\0'),
                _ => {
                    return i
                        .advance(char_offset)
                        .expected("valid escape (\\n, \\r, \\t, \\\\, \\', \\\", or \\0)")
                }
            },
            x => s.push(x),
        }
    }
    i.expected("end of string literal (\")")
}
