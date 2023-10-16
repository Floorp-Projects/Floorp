import { ExpressionBuilder } from '../expression.js';

/* @returns an ExpressionBuilder that evaluates a binary operation */
export function binary(op: string): ExpressionBuilder {
  return values => {
    const values_str = values.map(v => `(${v})`);
    return `(${values_str.join(op)})`;
  };
}
