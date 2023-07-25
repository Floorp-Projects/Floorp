/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use fluent::FluentResource;
use fluent_syntax::ast;
use nsstring::nsCString;
use thin_vec::ThinVec;

#[repr(C)]
pub struct TextElementInfo {
    id: nsCString,
    attr: nsCString,
    text: nsCString,
}

struct TextElementsCollector<'a> {
    current_id: Option<String>,
    current_attr: Option<String>,
    elements: &'a mut ThinVec<TextElementInfo>,
}

impl<'a> TextElementsCollector<'a> {
    pub fn new(elements: &'a mut ThinVec<TextElementInfo>) -> Self {
        Self {
            current_id: None,
            current_attr: None,
            elements: elements,
        }
    }

    fn collect_inline_expression(&mut self, x: &ast::InlineExpression<&str>) {
        match x {
            ast::InlineExpression::StringLiteral { .. } => {}
            ast::InlineExpression::NumberLiteral { .. } => {}
            ast::InlineExpression::FunctionReference { arguments, .. } => {
                self.collect_call_arguments(arguments);
            }
            ast::InlineExpression::MessageReference { .. } => {}
            ast::InlineExpression::TermReference { arguments, .. } => {
                if let Some(y) = arguments {
                    self.collect_call_arguments(y);
                }
            }
            ast::InlineExpression::VariableReference { .. } => {}
            ast::InlineExpression::Placeable { expression } => {
                self.collect_expression(expression.as_ref());
            }
        }
    }

    fn collect_named_argument(&mut self, x: &ast::NamedArgument<&str>) {
        self.collect_inline_expression(&x.value);
    }

    fn collect_call_arguments(&mut self, x: &ast::CallArguments<&str>) {
        for y in x.positional.iter() {
            self.collect_inline_expression(y);
        }
        for y in x.named.iter() {
            self.collect_named_argument(y);
        }
    }

    fn collect_variant(&mut self, x: &ast::Variant<&str>) {
        self.collect_pattern(&x.value);
    }

    fn collect_expression(&mut self, x: &ast::Expression<&str>) {
        match x {
            ast::Expression::Select { selector, variants } => {
                self.collect_inline_expression(selector);
                for y in variants.iter() {
                    self.collect_variant(y);
                }
            }
            ast::Expression::Inline(i) => {
                self.collect_inline_expression(i);
            }
        }
    }

    fn collect_pattern_element(&mut self, x: &ast::PatternElement<&str>) {
        match x {
            ast::PatternElement::TextElement { value } => {
                self.elements.push(TextElementInfo {
                    id: self
                        .current_id
                        .as_ref()
                        .map_or_else(|| nsCString::new(), nsCString::from),
                    attr: self
                        .current_attr
                        .as_ref()
                        .map_or_else(|| nsCString::new(), nsCString::from),
                    text: nsCString::from(*value),
                });
            }
            ast::PatternElement::Placeable { expression } => {
                self.collect_expression(expression);
            }
        }
    }

    fn collect_pattern(&mut self, x: &ast::Pattern<&str>) {
        for y in x.elements.iter() {
            self.collect_pattern_element(y);
        }
    }

    fn collect_attribute(&mut self, x: &ast::Attribute<&str>) {
        self.current_attr = Some(x.id.name.to_string());

        self.collect_pattern(&x.value);
    }

    fn collect_message(&mut self, x: &ast::Message<&str>) {
        self.current_id = Some(x.id.name.to_string());
        self.current_attr = None;

        if let Some(ref y) = x.value {
            self.collect_pattern(y);
        }
        for y in x.attributes.iter() {
            self.collect_attribute(y);
        }
    }

    fn collect_term(&mut self, x: &ast::Term<&str>) {
        self.current_id = Some(x.id.name.to_string());
        self.current_attr = None;

        self.collect_pattern(&x.value);
        for y in x.attributes.iter() {
            self.collect_attribute(y);
        }
    }

    pub fn collect_entry(&mut self, x: &ast::Entry<&str>) {
        match x {
            ast::Entry::Message(m) => {
                self.collect_message(m);
            }
            ast::Entry::Term(t) => {
                self.collect_term(t);
            }
            ast::Entry::Comment(_) => {}
            ast::Entry::GroupComment(_) => {}
            ast::Entry::ResourceComment(_) => {}
            ast::Entry::Junk { .. } => {}
        }
    }
}

#[no_mangle]
pub extern "C" fn fluent_resource_get_text_elements(
    res: &FluentResource,
    elements: &mut ThinVec<TextElementInfo>,
) {
    let mut collector = TextElementsCollector::new(elements);

    for entry in res.entries() {
        collector.collect_entry(entry);
    }
}
