/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanCategory {
  /**
   * Get a metric by name.
   *
   * Returns an object of the corresponding metric type,
   * with only the allowed functions available.
   */
  getter GleanMetric (DOMString identifier);
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanImpl {
  /**
   * Get a metric category by name.
   *
   * Returns an object for further metric lookup.
   */
  getter GleanCategory (DOMString identifier);
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanLabeled {
  /**
   * Get a specific metric for a given label.
   *
   * If a set of acceptable labels were specified in the `metrics.yaml` file,
   * and the given label is not in the set, it will be recorded under the
   * special `OTHER_LABEL` label.
   *
   * If a set of acceptable labels was not specified in the `metrics.yaml` file,
   * only the first 16 unique labels will be used.
   * After that, any additional labels will be recorded under the special
   * `OTHER_LABEL` label.
   */
  getter GleanMetric (DOMString identifier);
};
