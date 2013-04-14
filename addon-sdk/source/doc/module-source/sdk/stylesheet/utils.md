<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Module provides helper functions for working with stylesheets.

<api name="loadSheet">
  @function
  Synchronously loads a style sheet from `uri` and adds it to the list of
  additional style sheets of the document.
  The sheets added takes effect immediately, and only on the document of the
  `window` given.
  @param window {nsIDOMWindow}
  @param uri {string, nsIURI}
  @param [type="author"] {string}
    The type of the sheet. It accepts the following values: `"agent"`, `"user"`
    and `"author"`.
    If not provided, the default value is `"author"`.
</api>

<api name="removeSheet">
  @function
  Remove the document style sheet at `sheetURI` from the list of additional
  style sheets of the document.  The removal takes effect immediately.
  @param window {nsIDOMWindow}
  @param uri {string, nsIURI}
  @param [type="author"] {string}
    The type of the sheet. It accepts the following values: `"agent"`, `"user"`
    and `"author"`.
    If not provided, the default value is `"author"`.
</api>

<api name="isTypeValid">
  @function
  Verifies that the `type` given is a valid stylesheet's type.
  The values considered valid are: `"agent"`, `"user"` and `"author"`.
  @param type {string}
    The type of the sheet.
  @returns {boolean}
    `true` if the `type` given is valid, otherwise `false`.
</api>
