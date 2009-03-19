$(shell \
mkdir subd1; \
touch subd1/test.in; \
)

VVAR = %.in subd1

vpath $(VVAR)

all: test.in
	test "$<" = "subd1/test.in"
	@echo TEST-PASS
