# Makefile for ESC in Java
# Targets:
# all -- to build all parts.
# input, lexer, parser, semantics, generator -- to build a specfic part.
# sanity -- to run the compiler against a simple script. If things are
# working the compiler will return the value: completion( 0, okay, null )


all: util input lexer parser semantics generator runtime main sanity

main:
	javac -d Debug -classpath Debug ../../src/java/main/*.java 

util:
	javac -d Debug -classpath Debug ../../src/java/util/*.java

input:
	javac -d Debug -classpath Debug ../../src/java/input/*.java

lexer:
	javac -d Debug -classpath Debug ../../src/java/lexer/*.java 

parser:
	javac -d Debug -classpath Debug ../../src/java/parser/*.java 

semantics:
	javac -d Debug -classpath Debug ../../src/java/semantics/*.java ../../src/java/semantics/values/*.java ../../src/java/semantics/types/*.java

generator:
	javac -d Debug -classpath Debug ../../src/java/generator/*.java 

runtime:
	javac -d Debug -classpath Debug ../../src/java/runtime/*.java 

testcase:
	javac -d Debug -classpath Debug Script1.java
	java -classpath Debug Test1 Script1

sanity:
	java -classpath Debug Main ../../test/sanity.js

test:
	java -classpath Debug Main -d ../../test/ecma-e4/02.expressions/primary.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/02.expressions/prefixunary.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/02.expressions/postfixunary.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/02.expressions/binary.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/break.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/continue.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/do.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/for.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/forin.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/if.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/labeled.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/return.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/switch.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/throw.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/try.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/while.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/03.statements/with.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/04.definitions/definition.1.js
	java -classpath Debug Main -d ../../test/ecma-e4/04.definitions/definition.2.js

test_functions:
	java -classpath Debug Main -debug ../../test/ecma-e4/06.functions/function.1.js

