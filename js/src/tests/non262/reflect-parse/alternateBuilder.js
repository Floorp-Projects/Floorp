// |reftest| skip-if(!xulRuntime.shell)
// A simple proof-of-concept that the builder API can be used to generate other
// formats, such as JsonMLAst:
//
//     http://code.google.com/p/es-lab/wiki/JsonMLASTFormat
//
// It's incomplete (e.g., it doesn't convert source-location information and
// doesn't use all the direct-eval rules), but I think it proves the point.

function test() {

var JsonMLAst = (function() {
function reject() {
    throw new SyntaxError("node type not supported");
}

function isDirectEval(expr) {
    // an approximation to the actual rules. you get the idea
    return (expr[0] === "IdExpr" && expr[1].name === "eval");
}

function functionNode(type) {
    return function(id, args, body, isGenerator, isExpression) {
        if (isExpression)
            body = ["ReturnStmt", {}, body];

        if (!id)
            id = ["Empty"];

        // Patch up the argument node types: s/IdExpr/IdPatt/g
        for (var i = 0; i < args.length; i++) {
            args[i][0] = "IdPatt";
        }

        args.unshift("ParamDecl", {});

        return [type, {}, id, args, body];
    }
}

return {
    program: function(stmts) {
        stmts.unshift("Program", {});
        return stmts;
    },
    identifier: function(name) {
        return ["IdExpr", { name: name }];
    },
    literal: function(val) {
        return ["LiteralExpr", { value: val }];
    },
    expressionStatement: expr => expr,
    conditionalExpression: function(test, cons, alt) {
        return ["ConditionalExpr", {}, test, cons, alt];
    },
    unaryExpression: function(op, expr) {
        return ["UnaryExpr", {op: op}, expr];
    },
    binaryExpression: function(op, left, right) {
        return ["BinaryExpr", {op: op}, left, right];
    },
    property: function(kind, key, val) {
        return [kind === "init"
                ? "DataProp"
                : kind === "get"
                ? "GetterProp"
                : "SetterProp",
                {name: key[1].name}, val];
    },
    functionDeclaration: functionNode("FunctionDecl"),
    variableDeclaration: function(kind, elts) {
        if (kind === "let" || kind === "const")
            throw new SyntaxError("let and const not supported");
        elts.unshift("VarDecl", {});
        return elts;
    },
    variableDeclarator: function(id, init) {
        id[0] = "IdPatt";
        if (!init)
            return id;
        return ["InitPatt", {}, id, init];
    },
    sequenceExpression: function(exprs) {
        var length = exprs.length;
        var result = ["BinaryExpr", {op:","}, exprs[exprs.length - 2], exprs[exprs.length - 1]];
        for (var i = exprs.length - 3; i >= 0; i--) {
            result = ["BinaryExpr", {op:","}, exprs[i], result];
        }
        return result;
    },
    assignmentExpression: function(op, lhs, expr) {
        return ["AssignExpr", {op: op}, lhs, expr];
    },
    logicalExpression: function(op, left, right) {
        return [op === "&&" ? "LogicalAndExpr" : "LogicalOrExpr", {}, left, right];
    },
    updateExpression: function(expr, op, isPrefix) {
        return ["CountExpr", {isPrefix:isPrefix, op:op}, expr];
    },
    newExpression: function(callee, args) {
        args.unshift("NewExpr", {}, callee);
        return args;
    },
    callExpression: function(callee, args) {
        args.unshift(isDirectEval(callee) ? "EvalExpr" : "CallExpr", {}, callee);
        return args;
    },
    memberExpression: function(isComputed, expr, member) {
        return ["MemberExpr", {}, expr, isComputed ? member : ["LiteralExpr", {type: "string", value: member[1].name}]];
    },
    functionExpression: functionNode("FunctionExpr"),
    arrayExpression: function(elts) {
        for (var i = 0; i < elts.length; i++) {
            if (!elts[i])
                elts[i] = ["Empty"];
        }
        elts.unshift("ArrayExpr", {});
        return elts;
    },
    objectExpression: function(props) {
        props.unshift("ObjectExpr", {});
        return props;
    },
    thisExpression: function() {
        return ["ThisExpr", {}];
    },
    templateLiteral: function(elts) {
        for (var i = 0; i < elts.length; i++) {
            if (!elts[i])
                elts[i] = ["Empty"];
        }
        elts.unshift("TemplateLit", {});
        return elts;
    },

    yieldExpression: reject,

    emptyStatement: () => ["EmptyStmt", {}],
    blockStatement: function(stmts) {
        stmts.unshift("BlockStmt", {});
        return stmts;
    },
    labeledStatement: function(lab, stmt) {
        return ["LabelledStmt", {label: lab}, stmt];
    },
    ifStatement: function(test, cons, alt) {
        return ["IfStmt", {}, test, cons, alt || ["EmptyStmt", {}]];
    },
    switchStatement: function(test, clauses, isLexical) {
        clauses.unshift("SwitchStmt", {}, test);
        return clauses;
    },
    whileStatement: function(expr, stmt) {
        return ["WhileStmt", {}, expr, stmt];
    },
    doWhileStatement: function(stmt, expr) {
        return ["DoWhileStmt", {}, stmt, expr];
    },
    forStatement: function(init, test, update, body) {
        return ["ForStmt", {}, init || ["Empty"], test || ["Empty"], update || ["Empty"], body];
    },
    forInStatement: function(lhs, rhs, body) {
        return ["ForInStmt", {}, lhs, rhs, body];
    },
    breakStatement: function(lab) {
        return lab ? ["BreakStmt", {}, lab] : ["BreakStmt", {}];
    },
    continueStatement: function(lab) {
        return lab ? ["ContinueStmt", {}, lab] : ["ContinueStmt", {}];
    },
    withStatement: function(expr, stmt) {
        return ["WithStmt", {}, expr, stmt];
    },
    returnStatement: function(expr) {
        return expr ? ["ReturnStmt", {}, expr] : ["ReturnStmt", {}];
    },
    tryStatement: function(body, handler, fin) {
        var node = ["TryStmt", body, handler];
        if (fin)
            node.push(fin);
        return node;
    },
    throwStatement: function(expr) {
        return ["ThrowStmt", {}, expr];
    },
    debuggerStatement: () => ["DebuggerStmt", {}],
    letStatement: reject,
    switchCase: function(expr, stmts) {
        if (expr)
            stmts.unshift("SwitchCase", {}, expr);
        else
            stmts.unshift("DefaultCase", {});
        return stmts;
    },
    catchClause: function(param, body) {
        param[0] = "IdPatt";
        return ["CatchClause", {}, param, body];
    },

    arrayPattern: reject,
    objectPattern: reject,
    propertyPattern: reject,
};
})();

Pattern(["Program", {},
         ["BinaryExpr", {op: "+"},
          ["LiteralExpr", {value: 2}],
          ["BinaryExpr", {op: "*"},
           ["UnaryExpr", {op: "-"}, ["IdExpr", {name: "x"}]],
           ["IdExpr", {name: "y"}]]]]).match(Reflect.parse("2 + (-x * y)", {loc: false, builder: JsonMLAst}));

}

runtest(test);
