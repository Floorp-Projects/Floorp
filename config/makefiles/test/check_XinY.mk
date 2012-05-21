# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Verify dependencies are available
$(call requiredfunction,getargv subargv is_XinY errorifneq)

ifdef VERBOSE
  $(warning loading test)
endif

zero := 0
one  := 1

# Verify 'invalid' is not matched
val  := invalid
$(call errorifneq,$(zero),$(words $(call is_XinY,foo,$(val))))
$(call errorifneq,$(zero),$(words $(call is_XinY,clean,$(val))))
$(call errorifneq,$(zero),$(words $(call is_XinY,clean%,$(val))))

# verify strcmp('clean')
val  := clean
$(call errorifneq,$(zero),$(words $(call is_XinY,foo,$(val))))
$(call errorifneq,$(one),$(words $(call is_XinY,clean,$(val))))
$(call errorifneq,$(one,$(words $(call is_XinY,clean%,$(val)))))
$(call errorifneq,$(one),$(words $(call is_XinY,%clean,$(val))))

# List match for 'clean'
val     := blah clean distclean FcleanG clean-level-1
wanted  := clean distclean clean-level-1
$(call errorifneq,$(zero),$(words $(call is_XinY_debug,foo,$(val))))
$(call errorifneq,$(one),$(words $(call is_XinY,clean,$(val))))
$(call errorifneq,$(one),$(words $(call is_XinY,distclean,$(val))))

# pattern match 'clean'
#     match: clean, distclean, clean-level-1
#   exclude: FcleanG
TEST_MAKECMDGOALS := $(val)
$(call errorifneq,3,$(words $(call isTargetStemClean)))

TEST_MAKECMDGOALS := invalid
$(call errorifneq,$(zero),$(words $(call isTargetStemClean)))


#############################
ifdef VERBOSE
  $(call banner,Unit test: isTargetStem)
endif

# Verify list argument processing
TEST_MAKECMDGOALS := echo
$(call errorifneq,$(one),$(words $(call isTargetStem,echo,show)))

TEST_MAKECMDGOALS := echo-123
$(call errorifneq,$(one),$(words $(call isTargetStem,echo,show)))

TEST_MAKECMDGOALS := show
$(call errorifneq,$(one),$(words $(call isTargetStem,echo,show)))

TEST_MAKECMDGOALS := show-123
$(call errorifneq,$(one),$(words $(call isTargetStem,echo,show)))

TEST_MAKECMDGOALS := show-123-echo
$(call errorifneq,$(one),$(words $(call isTargetStem,echo,show)))

TEST_MAKECMDGOALS := invalid
$(call errorifneq,$(zero),$(words $(call isTargetStem,echo,show)))

