/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/permissions/#permissions-interface
 *
 * This file is in chrome-webidl as:
 * 1. This is for webdriver and is not directly exposed to web
 * 2. Putting this to webidl/Permissions.webidl causes header inclusion conflict
 */

[GenerateInit]
dictionary PermissionSetParameters {
  required object descriptor;
  required PermissionState state;
};
