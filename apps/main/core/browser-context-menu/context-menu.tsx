/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function ContextMenu(id: string, l10n: string, runFunction: () => void) {
  return (
    <xul:menuitem
      data-l10n-id={l10n}
      label={l10n}
      id={id}
      onCommand={runFunction}
    />
  );
}
