# where do comments take effect?

VAR = val1 # comment
VAR2 = lit2\#hash
VAR3 = val3
VAR4 = lit4\\#backslash
VAR5 = lit5\char
# This comment extends to the next line \
VAR3 = ignored

all:
	test "$(VAR)" = "val1 "
	test "$(VAR2)" = "lit2#hash"
	test "$(VAR3)" = "val3"
	test '$(VAR4)' = 'lit4\'
	test '$(VAR5)' = 'lit5\char'
	@echo "TEST-PASS"
