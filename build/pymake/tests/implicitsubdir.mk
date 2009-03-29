$(shell \
mkdir foo; \
touch test.in \
)

all: foo/test.out
	@echo TEST-PASS

foo/%.out: %.in
	cp $< $@


