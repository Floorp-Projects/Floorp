# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

INTL_UNICHARUTIL_UTIL_LCPPSRCS = \
	nsUnicharUtils.cpp \
	nsBidiUtils.cpp \
	nsSpecialCasingData.cpp \
	nsUnicodeProperties.cpp \
	$(NULL)

INTL_UNICHARUTIL_UTIL_CPPSRCS = $(addprefix $(topsrcdir)/intl/unicharutil/util/, $(INTL_UNICHARUTIL_UTIL_LCPPSRCS))
