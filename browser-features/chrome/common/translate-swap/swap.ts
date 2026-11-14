// SPDX-License-Identifier: MPL-2.0

import type { LanguageSelection } from "./types.ts";

/**
 * Both menus must hold different languages, and each language must exist in the
 * list it is moving into: the source and target lists are not identical.
 */
export function canSwapLanguages(selection: LanguageSelection): boolean {
  const { from, to, fromOptions, toOptions } = selection;
  return (
    from !== "" &&
    to !== "" &&
    from !== to &&
    toOptions.includes(from) &&
    fromOptions.includes(to)
  );
}
