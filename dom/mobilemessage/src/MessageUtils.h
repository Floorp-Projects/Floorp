/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MessageUtils_h
#define MessageUtils_h

/**
 * A helper function to convert the JS value to an integer value for time.
 *
 * @params aCx
 *         The JS context.
 * @params aTime
 *         Can be an object or a number.
 * @params aReturn
 *         The integer value to return.
 * @return NS_OK if the convertion succeeds.
 */
static nsresult
convertTimeToInt(JSContext* aCx, const JS::Value& aTime, uint64_t& aReturn)
{
  if (aTime.isObject()) {
    JS::Rooted<JSObject*> timestampObj(aCx, &aTime.toObject());
    if (!JS_ObjectIsDate(aCx, timestampObj)) {
      return NS_ERROR_INVALID_ARG;
    }
    aReturn = js_DateGetMsecSinceEpoch(timestampObj);
  } else {
    if (!aTime.isNumber()) {
      return NS_ERROR_INVALID_ARG;
    }
    double number = aTime.toNumber();
    if (static_cast<uint64_t>(number) != number) {
      return NS_ERROR_INVALID_ARG;
    }
    aReturn = static_cast<uint64_t>(number);
  }
  return NS_OK;
}

#endif
