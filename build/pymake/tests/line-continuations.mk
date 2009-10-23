VAR = val1 	 \
  	  val2  

VAR2 = val1space\
val2

VAR3 = val3 \\\
	cont3

all: otarget test.target
	test "$(VAR)" = "val1 val2  "
	test "$(VAR2)" = "val1space val2"
	test '$(VAR3)' = 'val3 \ cont3'
	test "hello \
	  world" = "hello   world"
	test "hello" = \
"hello"
	@echo TEST-PASS

otarget: ; test "hello\
	world" = "helloworld"

test.target: %.target: ; test "hello\
	world" = "helloworld"
