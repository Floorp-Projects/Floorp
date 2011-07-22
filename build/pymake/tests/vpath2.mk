VPATH = foo bar

$(shell \
mkdir bar; touch bar/test.source; \
sleep 2; \
mkdir foo; touch foo/tfile1; \
touch bar/tfile2 bar/tfile3 bar/test.objtest; \
)

all: tfile1 tfile2 tfile3 test.objtest test.source
	test "$^" = "foo/tfile1 bar/tfile2 bar/tfile3 bar/test.objtest bar/test.source"
	@echo TEST-PASS

tfile3: test.objtest

%.objtest: %.source
	test "$<" = bar/test.source
	test "$@" = test.objtest
