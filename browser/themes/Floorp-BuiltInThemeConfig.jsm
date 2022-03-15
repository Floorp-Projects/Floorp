/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**ここには、ビルドインテーマの定義が記載されています。これを直接、Firefox のソースコードに追記してください。追記先は、./BuiltInThemeConfig.jsm です。
 *
 *Version 8.5.9 からは、カラーテーマの期限の記載がある場所をすべて削除する必要もあります。ビルドする前に確認してください。5/19みたいな日付の場所です。
 */


  [
    "floorp-edge@mozilla.org",
    {
      version: "1.1.0",
      path: "resource://builtin-themes/motion/",
    },
  ],
  [
    "floorp-lepton@mozilla.org",
    {
      version: "1.1.0",
      path: "resource://builtin-themes/lepton/",
    },
  ],
  [
    "kanaze-erua@floorp.ablaze.one",
    {
      version: "1.0",
      path: "resource://builtin-themes/erua/",
      expiry: "2022-04-15",
    },
  ],
  [
    "kanaze-aria@floorp.ablaze.one",
    {
      version: "1.0",
      path: "resource://builtin-themes/aria/",
      expiry: "2022-04-15",
    },
  ],
  [
    "kanaze-melt@floorp.ablaze.one",
    {
      version: "1.0",
      path: "resource://builtin-themes/melt/",
      expiry: "2022-04-15",
    },
  ],