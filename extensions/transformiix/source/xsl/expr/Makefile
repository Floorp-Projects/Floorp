target: exprlib

CC = g++

EXPR_PATH      = .
ROOT_PATH      = ../..
XML_PATH       = $(ROOT_PATH)/xml
DOM_PATH       = $(XML_PATH)/dom
BASE_PATH      = $(ROOT_PATH)/base
XSLUTIL_PATH   = $(PROCESSOR_PATH)/util
PROCESSOR_PATH = ..

INCLUDE_PATHS = -I $(EXPR_PATH) \
                -I $(DOM_PATH) \
                -I $(BASE_PATH) \
                -I $(XML_PATH) \
                -I $(XSLUTIL_PATH) \
                -I $(PROCESSOR_PATH) \
                -I-


EXPR_OBJS = AdditiveExpr.o \
            AttributeExpr.o \
            AttributeValueTemplate.o \
            BasicNodeExpr.o \
            BooleanResult.o \
            ElementExpr.o \
            ExprLexer.o \
            ExprParser.o \
            FilterExpr.o \
            IdentityExpr.o \
            LocationStep.o \
            MultiplicativeExpr.o \
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
	    WildCardExpr.o

FUNCTION_CALL_OBJS = \
            BooleanFunctionCall.o \
            ErrorFunctionCall.o \
            FunctionCall.o \
            NodeSetFunctionCall.o \
            StringFunctionCall.o

ALL_OBJS = $(EXPR_OBJS) \
           $(FUNCTION_CALL_OBJS) \

exprlib: $(ALL_OBJS)

AdditiveExpr.o:  Expr.h AdditiveExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c AdditiveExpr.cpp


AttributeExpr.o: Expr.h AttributeExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c AttributeExpr.cpp

AttributeValueTemplate.o: Expr.h AttributeValueTemplate.cpp
	$(CC) $(INCLUDE_PATHS) -c AttributeValueTemplate.cpp

BasicNodeExpr.o: Expr.h BasicNodeExpr.cpp
	$(CC) $(INCLUDE_PATHS) -c BasicNodeExpr.cpp

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


########################
## FunctionCall package
########################


BooleanFunctionCall.o: FunctionLib.h BooleanFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c BooleanFunctionCall.cpp

ErrorFunctionCall.o: FunctionLib.h ErrorFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c ErrorFunctionCall.cpp

FunctionCall.o: FunctionLib.h FunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c FunctionCall.cpp

NodeSetFunctionCall.o: FunctionLib.h NodeSetFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c NodeSetFunctionCall.cpp

StringFunctionCall.o: FunctionLib.h StringFunctionCall.cpp
	$(CC) $(INCLUDE_PATHS) -c StringFunctionCall.cpp
