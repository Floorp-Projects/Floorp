all:
	test 'Hello world.' = '$(if 1,Hello \
	  world.)'
	test '(Hello world.)' != '(Hello \
	  world.)'
	@echo TEST-PASS
