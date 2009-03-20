$(shell \
mkdir -p src/subd; \
mkdir subd; \
touch dummy; \
sleep 2; \
touch subd/test.out src/subd/test.in2; \
sleep 2; \
touch subd/test.out2 src/subd/test.in; \
sleep 2; \
touch subd/host_test.out subd/host_test.out2; \
sleep 2; \
touch host_prog; \
)

VPATH = src

all: prog host_prog prog dir/
	test "$@" = "all"
	test "$<" = "prog"
	test "$^" = "prog host_prog dir"
	test "$?" = "prog host_prog dir"
	test "$+" = "prog host_prog prog dir"
	test "$(@D)" = "."
	test "$(@F)" = "all"
	test "$(<D)" = "."
	test "$(<F)" = "prog"
	test "$(^D)" = ". . ."
	test "$(^F)" = "prog host_prog dir"
	test "$(?D)" = ". . ."
	test "$(?F)" = "prog host_prog dir"
	test "$(+D)" = ". . . ."
	test "$(+F)" = "prog host_prog prog dir"
	@echo TEST-PASS

dir/:
	test "$@" = "dir"
	test "$<" = ""
	test "$^" = ""
	test "$(@D)" = "."
	test "$(@F)" = "dir"
	mkdir $@

prog: subd/test.out subd/test.out2
	test "$@" = "prog"
	test "$<" = "subd/test.out"
	test "$^" = "subd/test.out subd/test.out2" # ^
	test "$?" = "subd/test.out subd/test.out2" # ?
	cat $<
	test "$$(cat $<)" = "remade"
	test "$$(cat $(word 2,$^))" = ""

host_prog: subd/host_test.out subd/host_test.out2
	@echo TEST-FAIL No need to remake

%.out: %.in dummy
	test "$@" = "subd/test.out"
	test "$*" = "subd/test"              # *
	test "$<" = "src/subd/test.in"       # <
	test "$^" = "src/subd/test.in dummy" # ^
	test "$?" = "src/subd/test.in"       # ?
	test "$+" = "src/subd/test.in dummy" # +
	test "$(@D)" = "subd"
	test "$(@F)" = "test.out"
	test "$(*D)" = "subd"
	test "$(*F)" = "test"
	test "$(<D)" = "src/subd"
	test "$(<F)" = "test.in"
	test "$(^D)" = "src/subd ."          # ^D
	test "$(^F)" = "test.in dummy"
	test "$(?D)" = "src/subd"
	test "$(?F)" = "test.in"
	test "$(+D)" = "src/subd ."          # +D
	test "$(+F)" = "test.in dummy"
	printf "remade" >$@

%.out2: %.in2 dummy
	@echo TEST_FAIL No need to remake

.PHONY: all
