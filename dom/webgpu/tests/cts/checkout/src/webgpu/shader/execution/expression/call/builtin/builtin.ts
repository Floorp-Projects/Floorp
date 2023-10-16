import { ExpressionBuilder } from '../../expression.js';

/* @returns an ExpressionBuilder that calls the builtin with the given name */
export function builtin(name: string): ExpressionBuilder {
  return values => `${name}(${values.join(', ')})`;
}
