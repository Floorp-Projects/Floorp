unknown var = uval

all:
	test "$(subst a,b,value)" = "vblue"
	test "${subst a,b,va)lue}" = "vb)lue"
	test "$(subst /,\,ab/c)" = "ab\c"
	test '$(subst a,b,\\#)' = '\\#'
	test "$( subst a,b,value)" = ""
	test "$(Subst a,b,value)" = ""
	test "$(unknown var)" = "uval"
	@echo TEST-PASS
