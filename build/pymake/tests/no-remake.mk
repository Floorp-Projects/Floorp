$(shell date >testfile)

all: testfile
	@echo TEST-PASS

testfile:
	@echo TEST-FAIL "We shouldn't have remade this!"
