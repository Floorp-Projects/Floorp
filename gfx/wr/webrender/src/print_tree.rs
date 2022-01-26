/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io::Write;
use std::fmt::Write as FmtWrite;

/// A struct that makes it easier to print out a pretty tree of data, which
/// can be visually scanned more easily.
pub struct PrintTree<W>
where
    W: Write
{
    /// The current level of recursion.
    level: u32,

    /// An item which is queued up, so that we can determine if we need
    /// a mid-tree prefix or a branch ending prefix.
    queued_item: Option<String>,

    // We hold lines until they are done, and then output them all at
    // once
    line_buffer: String,

    /// The sink to print to.
    sink: W,
}

/// A trait that makes it easy to describe a pretty tree of data,
/// regardless of the printing destination, to either print it
/// directly to stdout, or serialize it as in the debugger
pub trait PrintTreePrinter {
    fn new_level(&mut self, title: String);
    fn end_level(&mut self);
    fn add_item(&mut self, text: String);
}

// The default does nothing but log
impl PrintTree<std::io::Sink> {
    pub fn new(title: &str) -> Self {
        PrintTree::new_with_sink(title, std::io::sink())
    }
}

impl<W> PrintTree<W>
where
    W: Write
{
    pub fn new_with_sink(title: &str, sink: W) -> Self {
        let mut result = PrintTree {
            level: 1,
            queued_item: None,
            line_buffer: String::new(),
            sink,
        };

        writeln!(result.line_buffer, "\u{250c} {}", title).unwrap();
        result.flush_line();
        result
    }

    fn print_level_prefix(&mut self) {
        for _ in 0 .. self.level {
            write!(self.line_buffer, "\u{2502}  ").unwrap();
        }
    }

    fn flush_queued_item(&mut self, prefix: &str) {
        if let Some(queued_item) = self.queued_item.take() {
            self.print_level_prefix();
            writeln!(self.line_buffer, "{} {}", prefix, queued_item).unwrap();
            self.flush_line();
        }
    }

    fn flush_line(&mut self) {
        debug!("{}", self.line_buffer);
        self.sink.write_all(self.line_buffer.as_bytes()).unwrap();
        self.line_buffer.clear();
    }
}

impl<W> PrintTreePrinter for PrintTree<W>
where
    W: Write
{
    /// Descend one level in the tree with the given title.
    fn new_level(&mut self, title: String) {
        self.flush_queued_item("\u{251C}\u{2500}");

        self.print_level_prefix();
        writeln!(self.line_buffer, "\u{251C}\u{2500} {}", title).unwrap();
        self.flush_line();

        self.level = self.level + 1;
    }

    /// Ascend one level in the tree.
    fn end_level(&mut self) {
        self.flush_queued_item("\u{2514}\u{2500}");
        self.level = self.level - 1;
    }

    /// Add an item to the current level in the tree.
    fn add_item(&mut self, text: String) {
        self.flush_queued_item("\u{251C}\u{2500}");
        self.queued_item = Some(text);
    }
}

impl<W> Drop for PrintTree<W>
where
    W: Write
{
    fn drop(&mut self) {
        self.flush_queued_item("\u{9492}\u{9472}");
    }
}

pub trait PrintableTree {
    fn print_with<T: PrintTreePrinter>(&self, pt: &mut T);
}
