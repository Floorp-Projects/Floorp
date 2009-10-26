# where do comments take effect?

VAR = val1 # comment
VAR2 = lit2\#hash
VAR2_1 = lit2.1\\\#hash
VAR3 = val3
VAR4 = lit4\\#backslash
VAR4_1 = lit4\\\\#backslash
VAR5 = lit5\char
VAR6 = lit6\\char
VAR7 = lit7\\
VAR8 = lit8\\\\
VAR9 = lit9\\\\extra
# This comment extends to the next line \
VAR3 = ignored

all:
	test "$(VAR)" = "val1 "
	test "$(VAR2)" = "lit2#hash"
	test '$(VAR2_1)' = 'lit2.1\#hash'
	test "$(VAR3)" = "val3"
	test '$(VAR4)' = 'lit4\'
	test '$(VAR4_1)' = 'lit4\\'
	test '$(VAR5)' = 'lit5\char'
	test '$(VAR6)' = 'lit6\\char'
	test '$(VAR7)' = 'lit7\\'
	test '$(VAR8)' = 'lit8\\\\'
	test '$(VAR9)' = 'lit9\\\\extra'
	@echo "TEST-PASS"
