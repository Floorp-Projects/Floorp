/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use percent_encoding::{percent_decode, percent_encode, NON_ALPHANUMERIC};
use std::str;

/// The `FragmentDirectiveParameter` represents one of
/// `[prefix-,]start[,end][,-suffix]` without any surrounding `-` or `,`.
///
/// The token is stored as percent-decoded string.
/// Therefore, interfaces exist to
///   - create a `FragmentDirectiveParameter` from a percent-encoded string.
///     This function will determine from occurrence and position of a dash
///     if the token represents a `prefix`, `suffix` or either `start` or `end`.
///   - create a percent-encoded string from the value the token holds.
pub enum TextDirectiveParameter {
    Prefix(String),
    StartOrEnd(String),
    Suffix(String),
}

impl TextDirectiveParameter {
    /// Creates a token from a percent-encoded string.
    /// Based on position of a dash the correct token type is determined.
    /// Returns `None` in case of an ill-formed token:
    ///   - starts and ends with a dash (i.e. `-token-`)
    ///   - only consists of a dash (i.e. `-`) or is empty
    ///   - conversion from percent-encoded string to utf8 fails.
    pub fn from_percent_encoded(token: &[u8]) -> Option<Self> {
        if token.is_empty() {
            return None;
        }
        let starts_with_dash = *token.first().unwrap() == b'-';
        let ends_with_dash = *token.last().unwrap() == b'-';
        if starts_with_dash && ends_with_dash {
            // `-token-` is not valid.
            return None;
        }
        if token.len() == 1 && starts_with_dash {
            // `-` is not valid.
            return None;
        }
        // Note: Trimming of the raw strings is currently not mentioned in the spec.
        // However, it looks as it is implicitly expected.
        if starts_with_dash {
            if let Ok(decoded_suffix) = percent_decode(&token[1..]).decode_utf8() {
                return Some(TextDirectiveParameter::Suffix(String::from(
                    decoded_suffix.trim(),
                )));
            }
            return None;
        }
        if ends_with_dash {
            if let Ok(decoded_prefix) = percent_decode(&token[..token.len() - 1]).decode_utf8() {
                return Some(TextDirectiveParameter::Prefix(String::from(
                    decoded_prefix.trim(),
                )));
            }
            return None;
        }
        if let Ok(decoded_text) = percent_decode(&token).decode_utf8() {
            return Some(TextDirectiveParameter::StartOrEnd(String::from(
                decoded_text.trim(),
            )));
        }
        None
    }

    /// Returns the value of the token as percent-decoded `String`.
    pub fn value(&self) -> &String {
        match self {
            TextDirectiveParameter::Prefix(value) => &value,
            TextDirectiveParameter::StartOrEnd(value) => &value,
            TextDirectiveParameter::Suffix(value) => &value,
        }
    }

    /// Creates a percent-encoded string of the token's value.
    /// This includes placing a dash appropriately
    /// to indicate whether this token is prefix, suffix or start/end.
    ///
    /// This method always returns a new object.
    pub fn to_percent_encoded_string(&self) -> String {
        let encode = |text: &String| percent_encode(text.as_bytes(), NON_ALPHANUMERIC).to_string();
        match self {
            Self::Prefix(text) => encode(text) + "-",
            Self::StartOrEnd(text) => encode(text),
            Self::Suffix(text) => {
                let encoded = encode(text);
                let mut result = String::with_capacity(encoded.len() + 1);
                result.push_str("-");
                result.push_str(&encoded);
                result
            }
        }
    }
}

/// This struct represents one parsed text directive using Rust types.
///
/// A text fragment is encoded into a URL fragment like this:
/// `text=[prefix-,]start[,end][,-suffix]`
///
/// The text directive is considered valid if at least `start` is not None.
/// (see `Self::is_valid()`).
#[derive(Default)]
pub struct TextDirective {
    prefix: Option<TextDirectiveParameter>,
    start: Option<TextDirectiveParameter>,
    end: Option<TextDirectiveParameter>,
    suffix: Option<TextDirectiveParameter>,
}
impl TextDirective {
    /// Creates an instance from string parts.
    /// This function is intended to be used when a fragment directive string should be created.
    /// Returns `None` if `start` is empty.
    pub fn from_parts(prefix: String, start: String, end: String, suffix: String) -> Option<Self> {
        if !start.is_empty() {
            Some(Self {
                prefix: if !prefix.is_empty() {
                    Some(TextDirectiveParameter::Prefix(prefix.trim().into()))
                } else {
                    None
                },
                start: Some(TextDirectiveParameter::StartOrEnd(start.trim().into())),
                end: if !end.is_empty() {
                    Some(TextDirectiveParameter::StartOrEnd(end.trim().into()))
                } else {
                    None
                },
                suffix: if !suffix.is_empty() {
                    Some(TextDirectiveParameter::Suffix(suffix.trim().into()))
                } else {
                    None
                },
            })
        } else {
            None
        }
    }

    /// Creates an instance from a percent-encoded string
    /// that originates from a fragment directive.
    ///
    /// `text_fragment` is supposed to have this format:
    /// ```
    /// text=[prefix-,]start[,end][,-suffix]
    /// ```
    /// This function returns `None` if `text_fragment`
    /// does not start with `text=`, it contains 0 or more
    /// than 4 elements or prefix/suffix/start or end
    /// occur too many times.
    /// It also returns `None` if any of the tokens parses to fail.
    pub fn from_percent_encoded_string(text_directive: &str) -> Option<Self> {
        // first check if the string starts with `text=`
        if text_directive.len() < 6 {
            return None;
        }
        if !text_directive.starts_with("text=") {
            return None;
        }

        let mut parsed_text_directive = Self::default();
        let valid = text_directive[5..]
            .split(",")
            // Parse the substrings into `TextDirectiveParameter`s. This will determine
            // for each substring if it is a Prefix, Suffix or Start/End,
            // or if it is invalid.
            .map(|token| TextDirectiveParameter::from_percent_encoded(token.as_bytes()))
            // populate `parsed_text_directive` and check its validity by inserting the parameters
            // one by one. Given that the parameters are sorted by their position in the source,
            // the validity of the text directive can be determined while adding the parameters.
            .map(|token| match token {
                Some(TextDirectiveParameter::Prefix(..)) => {
                    if !parsed_text_directive.is_empty() {
                        // `prefix-` must be the first result.
                        return false;
                    }
                    parsed_text_directive.prefix = token;
                    return true;
                }
                Some(TextDirectiveParameter::StartOrEnd(..)) => {
                    if parsed_text_directive.suffix.is_some() {
                        // start or end must come before `-suffix`.
                        return false;
                    }
                    if parsed_text_directive.start.is_none() {
                        parsed_text_directive.start = token;
                        return true;
                    }
                    if parsed_text_directive.end.is_none() {
                        parsed_text_directive.end = token;
                        return true;
                    }
                    // if `start` and `end` is already filled,
                    // this is invalid as well.
                    return false;
                }
                Some(TextDirectiveParameter::Suffix(..)) => {
                    if parsed_text_directive.start.is_some()
                        && parsed_text_directive.suffix.is_none()
                    {
                        // `start` must be present and `-suffix` must not be present.
                        // `end` may be present.
                        parsed_text_directive.suffix = token;
                        return true;
                    }
                    return false;
                }
                // empty or invalid token renders the whole text directive invalid.
                None => false,
            })
            .all(|valid| valid);
        if valid {
            return Some(parsed_text_directive);
        }
        None
    }

    /// Creates a percent-encoded string for the current `TextDirective`.
    /// In the unlikely case that the `TextDirective` is invalid (i.e. `start` is None),
    /// which should have been caught earlier,this method returns an empty string.
    pub fn to_percent_encoded_string(&self) -> String {
        if !self.is_valid() {
            return String::default();
        }
        String::from("text=")
            + &[&self.prefix, &self.start, &self.end, &self.suffix]
                .iter()
                .filter_map(|&token| token.as_ref())
                .map(|token| token.to_percent_encoded_string())
                .collect::<Vec<_>>()
                .join(",")
    }

    pub fn start(&self) -> &Option<TextDirectiveParameter> {
        &self.start
    }

    pub fn end(&self) -> &Option<TextDirectiveParameter> {
        &self.end
    }

    pub fn prefix(&self) -> &Option<TextDirectiveParameter> {
        &self.prefix
    }

    pub fn suffix(&self) -> &Option<TextDirectiveParameter> {
        &self.suffix
    }

    fn is_empty(&self) -> bool {
        self.prefix.is_none() && self.start.is_none() && self.end.is_none() && self.suffix.is_none()
    }

    /// A `TextDirective` object is valid if it contains the `start` token.
    /// All other tokens are optional.
    fn is_valid(&self) -> bool {
        self.start.is_some()
    }
}
/// Parses a fragment directive into a list of `TextDirective` objects and removes
/// the fragment directive from the input url.
///
/// If the hash does not contain a fragment directive, `url` is not modified
/// and this function returns `None`.
/// Otherwise, the fragment directive is removed from `url` and parsed.
/// If parsing fails, this function returns `None`.
pub fn parse_fragment_directive_and_remove_it_from_hash(
    url: &str,
) -> Option<(&str, &str, Vec<TextDirective>)> {
    // The Fragment Directive is preceded by a `:~:`,
    // which is only allowed to appear in the hash once.
    // However (even if unlikely), it might appear outside of the hash,
    // so this code only considers it when it is after the #.
    let maybe_first_hash_pos = url.find("#");
    // If there is no # in url, it is considered to be only the hash (and not a full url).
    let first_hash_pos = maybe_first_hash_pos.unwrap_or_default();
    let mut fragment_directive_iter = url[first_hash_pos..].split(":~:");
    let url_with_stripped_fragment_directive =
        &url[..first_hash_pos + fragment_directive_iter.next().unwrap_or_default().len()];

    if let Some(fragment_directive) = fragment_directive_iter.next() {
        if fragment_directive_iter.next().is_some() {
            // There are multiple occurrences of `:~:`, which is not allowed.
            return None;
        }
        // - fragments are separated by `&`.
        // - if a fragment does not start with `text=`, it is not a text fragment and will be ignored.
        // - if parsing of the text fragment fails (for whatever reason), it will be ignored.
        let text_directives: Vec<_> = fragment_directive
            .split("&")
            .map(|maybe_text_fragment| {
                TextDirective::from_percent_encoded_string(&maybe_text_fragment)
            })
            .filter_map(|maybe_text_directive| maybe_text_directive)
            .collect();
        if !text_directives.is_empty() {
            return Some((
                url_with_stripped_fragment_directive
                    .strip_suffix("#")
                    .unwrap_or(url_with_stripped_fragment_directive),
                fragment_directive,
                text_directives,
            ));
        }
    }
    None
}

/// Creates a percent-encoded text fragment string.
///
/// The returned string starts with `:~:`, so that it can be appended
/// to a normal fragment.
/// Text directives which are not valid (ie., they are missing the `start` parameter),
/// are skipped.
///
/// Returns `None` if `fragment_directives` is empty.
pub fn create_fragment_directive_string(text_directives: &Vec<TextDirective>) -> Option<String> {
    if text_directives.is_empty() {
        return None;
    }
    let encoded_fragment_directives: Vec<_> = text_directives
        .iter()
        .filter(|&fragment_directive| fragment_directive.is_valid())
        .map(|fragment_directive| fragment_directive.to_percent_encoded_string())
        .filter(|text_directive| !text_directive.is_empty())
        .collect();
    if encoded_fragment_directives.is_empty() {
        return None;
    }
    Some(String::from(":~:") + &encoded_fragment_directives.join("&"))
}

/// Creates the percent-encoded text directive string for a single text directive.
pub fn create_text_directive_string(text_directive: &TextDirective) -> Option<String> {
    if text_directive.is_valid() {
        Some(text_directive.to_percent_encoded_string())
    } else {
        None
    }
}
