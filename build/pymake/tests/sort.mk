# sort should remove duplicates
all:
	@test "$(sort x a y b z c a z b x c y)" = "a b c x y z"
	@echo "TEST-PASS"
