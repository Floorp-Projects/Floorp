/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * @param  {Number} numbers that will be summed.
 * @returns {String} A string of the following form: `${arg1} + ${arg2} ${argn} = ${sum}`
 */
function sum(...args) {
  return `${args.join(" + ")} = ${args.reduce((acc, i) => acc + i)}`;
}

export { sum };
