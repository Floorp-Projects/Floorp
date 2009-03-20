VPATH = foo bar

$(shell \
mkdir foo; touch foo/tfile1; \
mkdir bar; touch bar/tfile2 bar/tfile3 bar/test.objtest; \
sleep 2; \
touch bar/test.source; \
)

all: tfile1 tfile2 tfile3 test.objtest test.source
	test "$^" = "foo/tfile1 bar/tfile2 tfile3 test.objtest bar/test.source"
	@echo TEST-PASS

tfile3: test.objtest

%.objtest: %.source
	test "$<" = bar/test.source
	test "$@" = test.objtest
