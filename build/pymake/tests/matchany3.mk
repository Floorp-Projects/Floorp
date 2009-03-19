$(shell \
echo "target" > target.in; \
)

all: target
	test "$$(cat $^)" = "target"
	@echo TEST-PASS

%: %.in
	cp $< $@
