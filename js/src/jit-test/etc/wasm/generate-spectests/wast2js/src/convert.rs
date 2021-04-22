/* Copyright 2021 Mozilla Foundation
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

use anyhow::{bail, Context as _, Result};
use std::fmt::Write;
use std::path::Path;
use std::str;

const HARNESS: &'static str = include_str!("./harness.js");
const LICENSE: &'static str = include_str!("./license.js");

pub fn harness() -> String {
    format_js(Path::new("harness.js"), HARNESS)
}

pub fn convert<P: AsRef<Path>>(path: P, wast: &str) -> Result<String> {
    let filename = path.as_ref();
    let adjust_wast = |mut err: wast::Error| {
        err.set_path(filename);
        err.set_text(wast);
        err
    };

    let buf = wast::parser::ParseBuffer::new(wast).map_err(adjust_wast)?;
    let ast = wast::parser::parse::<wast::Wast>(&buf).map_err(adjust_wast)?;

    let mut out = String::new();

    writeln!(&mut out, "{}", LICENSE)?;
    writeln!(&mut out, "// {}\n", filename.display())?;

    let mut current_instance: Option<usize> = None;

    for directive in ast.directives {
        let sp = directive.span();
        let (line, col) = sp.linecol_in(wast);
        convert_directive(
            directive,
            &mut current_instance,
            filename,
            line,
            col,
            wast,
            &mut out,
        )
        .with_context(|| {
            format!(
                "failed to convert directive on {}:{}:{}",
                filename.display(),
                line + 1,
                col
            )
        })?;
    }

    Ok(format_js(Path::new(filename), &out))
}

fn format_js(filename: &Path, text: &str) -> String {
    let config = dprint_plugin_typescript::configuration::ConfigurationBuilder::new()
        .deno()
        .build();
    std::panic::catch_unwind(|| {
        dprint_plugin_typescript::format_text(filename, text, &config).unwrap()
    })
    .unwrap_or_else(|_| text.to_string())
}

fn convert_directive(
    directive: wast::WastDirective,
    current_instance: &mut Option<usize>,
    filename: &Path,
    line: usize,
    col: usize,
    wast: &str,
    out: &mut String,
) -> Result<()> {
    use wast::WastDirective::*;

    if col == 1 {
        writeln!(out, "// {}:{}", filename.display(), line + 1)?;
    } else {
        writeln!(out, "// {}:{}:{}", filename.display(), line + 1, col)?;
    }
    match directive {
        Module(module) => {
            let next_instance = current_instance.map(|x| x + 1).unwrap_or(0);
            let module_text = module_to_js_string(&module, wast)?;

            writeln!(
                out,
                "let ${} = instantiate(`{}`);",
                next_instance, module_text
            )?;
            if let Some(id) = module.id {
                writeln!(
                    out,
                    "register(${}, {});",
                    next_instance,
                    format!("`{}`", escape_template_string(id.name()))
                )?;
            }

            *current_instance = Some(next_instance);
        }
        QuoteModule { span: _, source: _ } => {
            write!(out, "// unsupported quote module")?;
        }
        Register {
            span: _,
            name,
            module,
        } => {
            let instanceish = module
                .map(|x| format!("`{}`", escape_template_string(x.name())))
                .unwrap_or_else(|| format!("${}", current_instance.unwrap()));

            writeln!(
                out,
                "register({}, `{}`);",
                instanceish,
                escape_template_string(name)
            )?;
        }
        Invoke(i) => {
            writeln!(out, "{};", invoke_to_js(current_instance, i)?)?;
        }
        AssertReturn {
            span: _,
            exec,
            results,
        } => {
            writeln!(
                out,
                "assert_return(() => {}, {});",
                execute_to_js(current_instance, exec, wast)?,
                to_js_value_array(&results, |x| assert_expression_to_js_value(x))?
            )?;
        }
        AssertTrap {
            span: _,
            exec,
            message,
        } => {
            writeln!(
                out,
                "assert_trap(() => {}, `{}`);",
                execute_to_js(current_instance, exec, wast)?,
                escape_template_string(message)
            )?;
        }
        AssertExhaustion {
            span: _,
            call,
            message,
        } => {
            writeln!(
                out,
                "assert_exhaustion(() => {}, `{}`);",
                invoke_to_js(current_instance, call)?,
                escape_template_string(message)
            )?;
        }
        AssertInvalid {
            span: _,
            module,
            message,
        } => {
            let text = module_to_js_string(&module, wast)?;
            writeln!(
                out,
                "assert_invalid(() => instantiate(`{}`), `{}`);",
                text,
                escape_template_string(message)
            )?;
        }
        AssertMalformed {
            module,
            span: _,
            message,
        } => {
            let text = match module {
                wast::QuoteModule::Module(m) => module_to_js_string(&m, wast)?,
                wast::QuoteModule::Quote(source) => quote_module_to_js_string(source)?,
            };
            writeln!(
                out,
                "assert_malformed(() => instantiate(`{}`), `{}`);",
                text,
                escape_template_string(message)
            )?;
        }
        AssertUnlinkable {
            span: _,
            module,
            message,
        } => {
            let text = module_to_js_string(&module, wast)?;
            writeln!(
                out,
                "assert_unlinkable(() => instantiate(`{}`), `{}`);",
                text,
                escape_template_string(message)
            )?;
        }
    }
    writeln!(out, "")?;

    Ok(())
}

fn escape_template_string(text: &str) -> String {
    text.replace("$", "$$")
        .replace("\\", "\\\\")
        .replace("`", "\\`")
}

fn span_to_offset(span: wast::Span, text: &str) -> Result<usize> {
    let (span_line, span_col) = span.linecol_in(text);
    let mut cur = 0;
    // Use split_terminator instead of lines so that if there is a `\r`,
    // it is included in the offset calculation. The `+1` values below
    // account for the `\n`.
    for (i, line) in text.split_terminator('\n').enumerate() {
        if span_line == i {
            assert!(span_col < line.len());
            return Ok(cur + span_col);
        }
        cur += line.len() + 1;
    }
    bail!("invalid line/col");
}

fn closed_module(module: &str) -> Result<&str> {
    enum State {
        Module,
        QStr,
        EscapeQStr,
    }

    let mut i = 0;
    let mut level = 1;
    let mut state = State::Module;

    let mut chars = module.chars();
    while level != 0 {
        let next = match chars.next() {
            Some(next) => next,
            None => bail!("was unable to close module"),
        };
        match state {
            State::Module => match next {
                '(' => level += 1,
                ')' => level -= 1,
                '"' => state = State::QStr,
                _ => {}
            },
            State::QStr => match next {
                '"' => state = State::Module,
                '\\' => state = State::EscapeQStr,
                _ => {}
            },
            State::EscapeQStr => match next {
                _ => state = State::QStr,
            },
        }
        i += next.len_utf8();
    }
    return Ok(&module[0..i]);
}

fn module_to_js_string(module: &wast::Module, wast: &str) -> Result<String> {
    let offset = span_to_offset(module.span, wast)?;
    let opened_module = &wast[offset..];
    if !opened_module.starts_with("module") {
        return Ok(escape_template_string(opened_module));
    }
    Ok(escape_template_string(&format!(
        "({}",
        closed_module(opened_module)?
    )))
}

fn quote_module_to_js_string(quotes: Vec<&[u8]>) -> Result<String> {
    let mut text = String::new();
    for src in quotes {
        text.push_str(str::from_utf8(src)?);
        text.push_str(" ");
    }
    let escaped = escape_template_string(&text);
    Ok(escaped)
}

fn invoke_to_js(current_instance: &Option<usize>, i: wast::WastInvoke) -> Result<String> {
    let instanceish = i
        .module
        .map(|x| format!("`{}`", escape_template_string(x.name())))
        .unwrap_or_else(|| format!("${}", current_instance.unwrap()));

    Ok(format!(
        "invoke({}, `{}`, {})",
        instanceish,
        escape_template_string(i.name),
        to_js_value_array(&i.args, |x| expression_to_js_value(x))?
    ))
}

fn execute_to_js(
    current_instance: &Option<usize>,
    exec: wast::WastExecute,
    wast: &str,
) -> Result<String> {
    match exec {
        wast::WastExecute::Invoke(invoke) => invoke_to_js(current_instance, invoke),
        wast::WastExecute::Module(module) => {
            let text = module_to_js_string(&module, wast)?;
            Ok(format!("instantiate(`{}`)", text))
        }
        wast::WastExecute::Get { module, global } => {
            let instanceish = module
                .map(|x| format!("`{}`", escape_template_string(x.name())))
                .unwrap_or_else(|| format!("${}", current_instance.unwrap()));

            Ok(format!("get({}, `{}`)", instanceish, global))
        }
    }
}

fn to_js_value_array<T, F: Fn(&T) -> Result<String>>(vals: &[T], func: F) -> Result<String> {
    let mut out = String::new();
    let mut first = true;
    write!(out, "[")?;
    for val in vals {
        if !first {
            write!(out, ", ")?;
        }
        first = false;
        write!(out, "{}", (func)(val)?)?;
    }
    write!(out, "]")?;
    Ok(out)
}

fn f32_needs_bits(a: f32) -> bool {
    if a.is_infinite() {
        return false;
    }
    return a.is_nan()
        || ((a as f64) as f32).to_bits() != a.to_bits()
        || (format!("{:.}", a).parse::<f64>().unwrap() as f32).to_bits() != a.to_bits();
}

fn f64_needs_bits(a: f64) -> bool {
    return a.is_nan();
}

fn f32x4_needs_bits(a: &[wast::Float32; 4]) -> bool {
    a.iter().any(|x| {
        let as_f32 = f32::from_bits(x.bits);
        f32_needs_bits(as_f32)
    })
}

fn f64x2_needs_bits(a: &[wast::Float64; 2]) -> bool {
    a.iter().any(|x| {
        let as_f64 = f64::from_bits(x.bits);
        f64_needs_bits(as_f64)
    })
}

fn f32_to_js_value(val: f32) -> String {
    if val == f32::INFINITY {
        format!("Infinity")
    } else if val == f32::NEG_INFINITY {
        format!("-Infinity")
    } else if val.is_sign_negative() && val == 0f32 {
        format!("-0")
    } else {
        format!("{:.}", val)
    }
}

fn f64_to_js_value(val: f64) -> String {
    if val == f64::INFINITY {
        format!("Infinity")
    } else if val == f64::NEG_INFINITY {
        format!("-Infinity")
    } else if val.is_sign_negative() && val == 0f64 {
        format!("-0")
    } else {
        format!("{:.}", val)
    }
}

fn float32_to_js_value(val: &wast::Float32) -> String {
    let as_f32 = f32::from_bits(val.bits);
    if f32_needs_bits(as_f32) {
        format!(
            "bytes('f32', {})",
            to_js_value_array(&val.bits.to_le_bytes(), |x| Ok(format!("0x{:x}", x))).unwrap()
        )
    } else {
        format!("value('f32', {})", f32_to_js_value(as_f32))
    }
}

fn float64_to_js_value(val: &wast::Float64) -> String {
    let as_f64 = f64::from_bits(val.bits);
    if f64_needs_bits(as_f64) {
        format!(
            "bytes('f64', {})",
            to_js_value_array(&val.bits.to_le_bytes(), |x| Ok(format!("0x{:x}", x))).unwrap()
        )
    } else {
        format!("value('f64', {})", f64_to_js_value(as_f64))
    }
}

fn f32_pattern_to_js_value(pattern: &wast::NanPattern<wast::Float32>) -> String {
    use wast::NanPattern::*;
    match pattern {
        CanonicalNan => format!("`canonical_nan`"),
        ArithmeticNan => format!("`arithmetic_nan`"),
        Value(x) => float32_to_js_value(x),
    }
}

fn f64_pattern_to_js_value(pattern: &wast::NanPattern<wast::Float64>) -> String {
    use wast::NanPattern::*;
    match pattern {
        CanonicalNan => format!("`canonical_nan`"),
        ArithmeticNan => format!("`arithmetic_nan`"),
        Value(x) => float64_to_js_value(x),
    }
}

fn assert_expression_to_js_value(v: &wast::AssertExpression<'_>) -> Result<String> {
    use wast::AssertExpression::*;

    Ok(match &v {
        I32(x) => format!("value('i32', {})", x),
        I64(x) => format!("value('i64', {}n)", x),
        F32(x) => f32_pattern_to_js_value(x),
        F64(x) => f64_pattern_to_js_value(x),
        RefNull(x) => match x {
            Some(wast::HeapType::Func) => format!("value('funcref', null)"),
            Some(wast::HeapType::Extern) => format!("value('externref', null)"),
            other => bail!(
                "couldn't convert ref.null {:?} to a js assertion value",
                other
            ),
        },
        RefExtern(x) => format!("value('externref', externref({}))", x),
        V128(x) => {
            use wast::V128Pattern::*;
            match x {
                I8x16(elements) => format!(
                    "i8x16({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}", x)))?
                ),
                I16x8(elements) => format!(
                    "i16x8({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}", x)))?
                ),
                I32x4(elements) => format!(
                    "i32x4({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}", x)))?
                ),
                I64x2(elements) => format!(
                    "i64x2({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}n", x)))?
                ),
                F32x4(elements) => {
                    let elements: Vec<String> = elements
                        .iter()
                        .map(|x| f32_pattern_to_js_value(x))
                        .collect();
                    format!(
                        "new F32x4Pattern({}, {}, {}, {})",
                        elements[0], elements[1], elements[2], elements[3]
                    )
                }
                F64x2(elements) => {
                    let elements: Vec<String> = elements
                        .iter()
                        .map(|x| f64_pattern_to_js_value(x))
                        .collect();
                    format!("new F64x2Pattern({}, {})", elements[0], elements[1])
                }
            }
        }
        other => bail!("couldn't convert {:?} to a js assertion value", other),
    })
}

fn expression_to_js_value(v: &wast::Expression<'_>) -> Result<String> {
    use wast::Instruction::*;

    if v.instrs.len() != 1 {
        bail!("too many instructions in {:?}", v);
    }
    Ok(match &v.instrs[0] {
        I32Const(x) => format!("{}", *x),
        I64Const(x) => format!("{}n", *x),
        F32Const(x) => float32_to_js_value(x),
        F64Const(x) => float64_to_js_value(x),
        V128Const(x) => {
            use wast::V128Const::*;
            match x {
                I8x16(elements) => format!(
                    "i8x16({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}", x)))?
                ),
                I16x8(elements) => format!(
                    "i16x8({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}", x)))?
                ),
                I32x4(elements) => format!(
                    "i32x4({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}", x)))?
                ),
                I64x2(elements) => format!(
                    "i64x2({})",
                    to_js_value_array(elements, |x| Ok(format!("0x{:x}n", x)))?
                ),
                F32x4(elements) => {
                    if f32x4_needs_bits(elements) {
                        format!(
                            "bytes('v128', {})",
                            to_js_value_array(&x.to_le_bytes(), |x| Ok(format!("0x{:x}", x)))?
                        )
                    } else {
                        format!(
                            "f32x4({})",
                            to_js_value_array(elements, |x| Ok(f32_to_js_value(f32::from_bits(
                                x.bits
                            ))))?
                        )
                    }
                }
                F64x2(elements) => {
                    if f64x2_needs_bits(elements) {
                        format!(
                            "bytes('v128', {})",
                            to_js_value_array(&x.to_le_bytes(), |x| Ok(format!("0x{:x}", x)))?
                        )
                    } else {
                        format!(
                            "f64x2({})",
                            to_js_value_array(elements, |x| Ok(f64_to_js_value(f64::from_bits(
                                x.bits
                            ))))?
                        )
                    }
                }
            }
        }
        RefNull(_) => format!("null"),
        RefExtern(x) => format!("externref({})", x),
        other => bail!("couldn't convert {:?} to a js value", other),
    })
}
