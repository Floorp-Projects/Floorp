$(shell \
touch libfoo.so \
)

all: -lfoo
	test "$<" = "libfoo.so"
	@echo TEST-PASS
