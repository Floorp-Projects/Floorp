$(shell \
mkdir foo; \
touch a.c b.c c.out foo/d.c; \
sleep 2; \
touch c.in; \
)

VPATH = foo

all: c.out prog
	cat $<
	test "$$(cat $<)" = "remadec.out"
	@echo TEST-PASS

*.out: %.out: %.in
	test "$@" = c.out
	test "$<" = c.in
	printf "remade$@" >$@

prog: *.c
	test "$^" = "a.c b.c"
	touch $@
