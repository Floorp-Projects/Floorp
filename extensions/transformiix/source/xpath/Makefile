target: xpath

CC := $(CC) -g

ROOT_PATH      = ..
XML_PATH       = $(ROOT_PATH)/xml
DOM_PATH       = $(XML_PATH)/dom
BASE_PATH      = $(ROOT_PATH)/base
PROCESSOR_PATH = $(ROOT_PATH)/xslt

INCLUDE_PATHS = -I. \
                -I$(DOM_PATH) \
                -I$(BASE_PATH) \
                -I$(XML_PATH) \
                -I$(PROCESSOR_PATH) \
                -I-


XPATH_OBJS = AdditiveExpr.o \
             AttributeExpr.o \
             AttributeValueTemplate.o \
             BasicNodeExpr.o \
             BooleanExpr.o \
             BooleanResult.o \
             ElementExpr.o \
             ExprLexer.o \
             ExprParser.o \
             FilterExpr.o \
             IdentityExpr.o \
             LocationStep.o \
             MultiplicativeExpr.o \
             NodeSet.o \
             NumberExpr.o \
             NumberResult.o \
             PredicateList.o \
             ParentExpr.o \
             PathExpr.o \
             RelationalExpr.o \
             RootExpr.o \
             StringExpr.o \
             StringResult.o \
	         TextExpr.o \
             UnionExpr.o \
             VariableRefExpr.o \
	         WildCardExpr.o \
             XPathNames.o

FUNCTION_CALL_OBJS = \
            BooleanFunctionCall.o \
            ErrorFunctionCall.o \
            ExtensionFunctionCall.o \
            FunctionCall.o \
            NodeSetFunctionCall.o \
            NumberFunctionCall.o \
            StringFunctionCall.o

ALL_OBJS = $(XPATH_OBJS) \
           $(FUNCTION_CALL_OBJS)

xpath: $(ALL_OBJS)

AdditiveExpr.o:  Expr.h AdditiveExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c AdditiveExpr.cpp


AttributeExpr.o: Expr.h AttributeExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c AttributeExpr.cpp

AttributeValueTemplate.o: Expr.h AttributeValueTemplate.cpp
	$(CC) $(INCLUDE_PATHS) -c AttributeValueTemplate.cpp

BasicNodeExpr.o: Expr.h BasicNodeExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c BasicNodeExpr.cpp

BooleanExpr.o: Expr.h BooleanExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c BooleanExpr.cpp

BooleanResult.o: ExprResult.h BooleanResult.cpp
	$(CC) $(INCLUDE_PATHS) -c BooleanResult.cpp

ElementExpr.o: Expr.h ElementExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c ElementExpr.cpp

ExprLexer.o: ExprLexer.cpp ExprLexer.h
	$(CC) $(INCLUDE_PATHS) -c ExprLexer.cpp

ExprParser.o: Expr.h ExprParser.cpp ExprParser.h
	$(CC) $(INCLUDE_PATHS) -c ExprParser.cpp

FilterExpr.o: FilterExpr.cpp Expr.h
	$(CC) $(INCLUDE_PATHS) -c FilterExpr.cpp

IdentityExpr.o: Expr.h IdentityExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c IdentityExpr.cpp

LocationStep.o: Expr.h LocationStep.cpp
	$(CC) $(INCLUDE_PATHS) -c LocationStep.cpp

MultiplicativeExpr.o: Expr.h MultiplicativeExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c MultiplicativeExpr.cpp

NodeSet.o: NodeSet.h NodeSet.cpp
	$(CC) $(INCLUDE_PATHS) -c NodeSet.cpp

NumberExpr.o: Expr.h  NumberExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c NumberExpr.cpp

NumberResult.o: ExprResult.h  NumberResult.cpp
	$(CC) $(INCLUDE_PATHS) -c NumberResult.cpp

ParentExpr.o: Expr.h ParentExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c ParentExpr.cpp

PathExpr.o: Expr.h PathExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c PathExpr.cpp

PredicateList.o: Expr.h PredicateList.cpp
	$(CC) $(INCLUDE_PATHS) -c PredicateList.cpp

RelationalExpr.o: Expr.h RelationalExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c RelationalExpr.cpp

RootExpr.o: Expr.h RootExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c RootExpr.cpp

StringExpr.o: Expr.h StringExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c StringExpr.cpp

StringResult.o: ExprResult.h StringResult.cpp
	$(CC) $(INCLUDE_PATHS) -c StringResult.cpp

TextExpr.o: Expr.h TextExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c TextExpr.cpp

UnionExpr.o: Expr.h UnionExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c UnionExpr.cpp

VariableRefExpr.o: Expr.h VariableRefExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c VariableRefExpr.cpp

WildCardExpr.o: Expr.h WildCardExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c WildCardExpr.cpp

XPathNames.o: FunctionLib.h XPathNames.cpp
	$(CC) $(INCLUDE_PATHS) -c XPathNames.cpp


FunctionCall.o: Expr.h FunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c FunctionCall.cpp

########################
## FunctionCall package
########################


BooleanFunctionCall.o: FunctionLib.h BooleanFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c BooleanFunctionCall.cpp

ErrorFunctionCall.o: FunctionLib.h ErrorFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c ErrorFunctionCall.cpp

ExtensionFunctionCall.o: FunctionLib.h ExtensionFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c ExtensionFunctionCall.cpp

NodeSetFunctionCall.o: FunctionLib.h NodeSetFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c NodeSetFunctionCall.cpp

NumberFunctionCall.o: FunctionLib.h NumberFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c NumberFunctionCall.cpp

StringFunctionCall.o: FunctionLib.h StringFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c StringFunctionCall.cpp




