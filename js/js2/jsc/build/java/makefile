# Makefile for JSC in Java
# Targets:
# all -- to build all parts.
# input, lexer, parser, semantics, generator -- to build a specfic part.
# sanity -- to run the compiler against a simple script. If things are
# working the compiler will return the value: completion( 0, okay, null )


all: input lexer parser semantics generator main sanity

main:
	javac -d classes -classpath classes ../../src/java/main/*.java 

input:
	javac -d classes -classpath classes ../../src/java/input/*.java

lexer:
	javac -d classes -classpath classes ../../src/java/lexer/*.java 

parser:
	javac -d classes -classpath classes ../../src/java/parser/*.java 

semantics:
	javac -d classes -classpath classes ../../src/java/semantics/*.java ../../src/java/semantics/values/*.java ../../src/java/semantics/types/*.java

generator:
	javac -d classes -classpath classes ../../src/java/generator/*.java 

sanity:
	java -classpath classes Main -a ../../test/sanity.js

testgen:
	java -classpath classes Main -d ../../test/ecma-e4/02.expressions/primary.1.js
	java -classpath classes Main -d ../../test/ecma-e4/06.functions/function.1.js

test:
	java -classpath classes Main -d ../../test/ecma-e4/02.expressions/primary.1.js
	java -classpath classes Main -d ../../test/ecma-e4/02.expressions/prefixunary.1.js
	java -classpath classes Main -d ../../test/ecma-e4/02.expressions/postfixunary.1.js
	java -classpath classes Main -d ../../test/ecma-e4/02.expressions/binary.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/break.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/continue.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/do.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/for.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/forin.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/if.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/labeled.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/return.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/switch.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/throw.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/try.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/while.1.js
	java -classpath classes Main -d ../../test/ecma-e4/03.statements/with.1.js
	java -classpath classes Main -d ../../test/ecma-e4/04.definitions/definition.1.js
	java -classpath classes Main -d ../../test/ecma-e4/04.definitions/definition.2.js

test_functions:
	java -classpath classes Main -debug -asm ../../test/ecma-e4/06.functions/function.1.js

