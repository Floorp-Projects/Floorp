/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-coverage.h:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Pango Library (www.pango.org).
 *
 * The Initial Developer of the Original Code is
 * Red Hat Software.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __PANGO_COVERAGE_H__
#define __PANGO_COVERAGE_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _PangoliteCoverage PangoliteCoverage;

typedef enum {
  PANGO_COVERAGE_NONE,
  PANGO_COVERAGE_FALLBACK,
  PANGO_COVERAGE_APPROXIMATE,
  PANGO_COVERAGE_EXACT
} PangoliteCoverageLevel;

PangoliteCoverage *    pangolite_coverage_new     (void);
PangoliteCoverage *    pangolite_coverage_ref     (PangoliteCoverage      *coverage);
void               pangolite_coverage_unref   (PangoliteCoverage      *coverage);
PangoliteCoverage *    pangolite_coverage_copy    (PangoliteCoverage      *coverage);
PangoliteCoverageLevel pangolite_coverage_get     (PangoliteCoverage      *coverage,
					   int                 index);
void               pangolite_coverage_set     (PangoliteCoverage      *coverage,
					   int                 index,
					   PangoliteCoverageLevel  level);
void               pangolite_coverage_max     (PangoliteCoverage      *coverage,
					   PangoliteCoverage      *other);

void           pangolite_coverage_to_bytes   (PangoliteCoverage  *coverage,
					  guchar        **bytes,
					  int            *n_bytes);
PangoliteCoverage *pangolite_coverage_from_bytes (guchar         *bytes,
					  int             n_bytes);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANGO_COVERAGE_H__ */




