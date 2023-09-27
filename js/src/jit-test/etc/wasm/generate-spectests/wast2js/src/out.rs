/* Copyright 2022 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/// This module has a bare-bones "syntax tree" that helps us output more readable JS code in our
/// tests. This is a lot cheaper and faster than running a full JS formatter on the generated code.
/// The tree only has node types for things that matter to formatting; most things in the output
/// use the Raw type, which is just a plain old string.

pub const LINE_WIDTH: usize = 80; // the default line width to try to meet (will not be met perfectly)
pub const NOWRAP: usize = 1000; // use for line_remaining when you don't want to wrap text

pub enum JSNode {
    Assert {
        name: String,
        exec: Box<JSNode>,
        expected: Box<JSNode>,
    },
    Invoke {
        instance: String,
        name: String,
        body: Box<JSNode>,
    },
    Call {
        name: String,
        args: Vec<Box<JSNode>>,
    },
    Array(Vec<Box<JSNode>>),
    Raw(String),
}

impl JSNode {
    /// Converts a node to JavaScript code. line_remaining is the number of characters remaining on
    /// the line, used for line-wrapping purposes; if zero, the default LINE_WIDTH will be used.
    pub fn output(&self, line_remaining: usize) -> String {
        let line_remaining = if line_remaining == 0 {
            LINE_WIDTH
        } else {
            line_remaining
        };
        match self {
            Self::Assert {
                name,
                exec,
                expected,
            } => {
                if self.len() > line_remaining {
                    format!(
                        "{}(\n{}\n)",
                        name,
                        indent(format!(
                            "() => {},\n{},",
                            exec.output(line_remaining - 8),
                            expected.output(line_remaining - 2),
                        )),
                    )
                } else {
                    format!(
                        "{}(() => {}, {})",
                        name,
                        exec.output(NOWRAP),
                        expected.output(NOWRAP),
                    )
                }
            }
            Self::Invoke {
                instance,
                name,
                body,
            } => {
                let len_before_body = "invoke".len() + instance.len() + name.len();
                if len_before_body > line_remaining {
                    format!(
                        "invoke({},\n{}\n)",
                        instance,
                        indent(format!("`{}`,\n{},", name, body.output(line_remaining - 2),)),
                    )
                } else {
                    let body_string = body.output(line_remaining - len_before_body);
                    format!("invoke({}, `{}`, {})", instance, name, body_string)
                }
            }
            Self::Call { name, args } => {
                if self.len() > line_remaining {
                    let arg_strings: Vec<String> = args
                        .iter()
                        .map(|arg| arg.output(line_remaining - 2))
                        .collect();
                    format!("{}(\n{},\n)", name, indent(arg_strings.join(",\n")))
                } else {
                    let arg_strings: Vec<String> =
                        args.iter().map(|arg| arg.output(NOWRAP)).collect();
                    format!("{}({})", name, arg_strings.join(", "))
                }
            }
            Self::Array(values) => {
                if self.len() > line_remaining {
                    let value_strings = output_nodes(&values, 70);
                    format!("[\n{},\n]", indent(value_strings.join(",\n")))
                } else {
                    let value_strings = output_nodes(&values, 80);
                    format!("[{}]", value_strings.join(", "))
                }
            }
            Self::Raw(val) => val.to_string(),
        }
    }

    /// A rough estimate of the string length of the node. Used as a heuristic to know when we
    /// should wrap text.
    fn len(&self) -> usize {
        match self {
            Self::Assert {
                name,
                exec,
                expected,
            } => name.len() + exec.len() + expected.len(),
            Self::Invoke {
                instance,
                name,
                body,
            } => instance.len() + name.len() + body.len(),
            Self::Call { name, args } => {
                let mut args_len: usize = 0;
                for node in args {
                    args_len += node.len() + ", ".len();
                }
                name.len() + args_len
            }
            Self::Array(nodes) => {
                let mut content_len: usize = 0;
                for node in nodes {
                    content_len += node.len() + ", ".len();
                }
                content_len
            }
            Self::Raw(s) => s.len(),
        }
    }
}

pub fn output_nodes(nodes: &Vec<Box<JSNode>>, line_width_per_node: usize) -> Vec<String> {
    nodes
        .iter()
        .map(|node| node.output(line_width_per_node))
        .collect()
}

pub fn strings_to_raws(strs: Vec<String>) -> Vec<Box<JSNode>> {
    let mut res: Vec<Box<JSNode>> = vec![];
    for s in strs {
        res.push(Box::new(JSNode::Raw(s)));
    }
    res
}

fn indent(s: String) -> String {
    let mut result = String::new();
    let mut do_indent = true;
    for (i, line) in s.lines().enumerate() {
        if i > 0 {
            result.push('\n');
        }
        if line.chars().any(|c| !c.is_whitespace()) {
            if do_indent {
                result.push_str("  ");
            }
            result.push_str(line);

            // An odd number of backticks in the line means we are entering or exiting a raw string.
            if line.matches("`").count() % 2 == 1 {
                do_indent = !do_indent
            }
        }
    }
    result
}
