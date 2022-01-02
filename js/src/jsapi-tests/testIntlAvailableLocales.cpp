/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/LocaleSensitive.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testIntlAvailableLocales) {
  JSRuntime* rt = JS_GetRuntime(cx);

  // This test should only attempt to run if we have Intl support.
  JS::Rooted<JS::Value> haveIntl(cx);
  EVAL("typeof Intl !== 'undefined'", &haveIntl);
  if (!haveIntl.toBoolean()) {
    return true;
  }

  // Assumption: our Intl support always includes "az" (Azerbaijani) support,
  // and our Intl support *does not* natively support az-Cyrl-AZ.
  CHECK(JS_SetDefaultLocale(rt, "az-Cyrl-AZ"));

  EXEC(
      "if (Intl.Collator().resolvedOptions().locale !== "
      "'az-Cyrl-AZ') \n"
      "    throw 'unexpected default locale';");
  EXEC(
      "var used = Intl.Collator('az-Cyrl').resolvedOptions().locale; \n"
      "if (used !== 'az-Cyrl') \n"
      "    throw 'bad locale when using truncated default: ' + used;");
  EXEC(
      "if (Intl.Collator('az').resolvedOptions().locale !== 'az') \n"
      "    throw 'bad locale when using more-truncated default';");
  EXEC(
      "if (Intl.Collator('az-Cyrl-US').resolvedOptions().locale !== 'az-Cyrl') "
      "\n"
      "    throw 'unexpected default locale';");

  EXEC(
      "if (Intl.Collator('az-Cyrl-AZ', { localeMatcher: 'lookup' "
      "}).resolvedOptions().locale !== \n"
      "    'az-Cyrl-AZ') \n"
      "{ \n"
      "    throw 'unexpected default locale with lookup matcher'; \n"
      "}");

  CHECK(JS_SetDefaultLocale(rt, "en-US-u-co-phonebk"));
  EXEC(
      "if (Intl.Collator().resolvedOptions().locale !== 'en-US') \n"
      "    throw 'unexpected default locale where proposed default included a "
      "Unicode extension';");

  CHECK(JS_SetDefaultLocale(rt, "this is not a language tag at all, yo"));

  EXEC(
      "if (Intl.Collator().resolvedOptions().locale !== 'en-GB') \n"
      "    throw 'unexpected last-ditch locale';");
  EXEC(
      "if (Intl.Collator('en-GB').resolvedOptions().locale !== 'en-GB') \n"
      "    throw 'unexpected used locale when specified, with last-ditch "
      "locale as default';");

  JS_ResetDefaultLocale(rt);
  return true;
}
END_TEST(testIntlAvailableLocales)
