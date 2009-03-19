all:
	test "$(patsubst foo,%.bar,foo)" = "%.bar"
	test "$(patsubst \%word,replace,word %word other)" = "word replace other"
	test "$(patsubst %.c,\%%.o,foo.c bar.o baz.cpp)" = "%foo.o bar.o baz.cpp"
	test "$(patsubst host_%.c,host_%.o,dir/host_foo.c host_bar.c)" = "dir/host_foo.c host_bar.o"
	test "$(patsubst foo,bar,dir/foo foo baz)" = "dir/foo bar baz"
	@echo TEST-PASS
