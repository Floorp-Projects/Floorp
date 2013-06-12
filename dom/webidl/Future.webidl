/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org/#futures
 */

interface FutureResolver {
  void resolve(optional any value);
  void reject(optional any value);
};

callback FutureInit = void (FutureResolver resolver);
callback AnyCallback = any (optional any value);

[Constructor(FutureInit init)]
interface Future {
  // TODO: update this interface - bug 875289
  void done(optional AnyCallback? resolveCallback = null,
            optional AnyCallback? rejectCallback = null);
};
