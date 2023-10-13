import {
  abstractFloatShaderBuilder,
  basicExpressionBuilder,
  ShaderBuilder,
} from '../expression.js';

/* @returns a ShaderBuilder that evaluates a prefix unary operation */
export function unary(op: string): ShaderBuilder {
  return basicExpressionBuilder(value => `${op}(${value})`);
}

/* @returns a ShaderBuilder that evaluates a prefix unary operation that returns AbstractFloats */
export function abstractUnary(op: string): ShaderBuilder {
  return abstractFloatShaderBuilder(value => `${op}(${value})`);
}
