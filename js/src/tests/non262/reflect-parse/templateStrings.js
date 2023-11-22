// |reftest| skip-if(!xulRuntime.shell)
function test() {

// template strings
assertStringExpr("`hey there`", literal("hey there"));
assertStringExpr("`hey\nthere`", literal("hey\nthere"));
assertExpr("`hey${\"there\"}`", templateLit([lit("hey"), lit("there"), lit("")]));
assertExpr("`hey${\"there\"}mine`", templateLit([lit("hey"), lit("there"), lit("mine")]));
assertExpr("`hey${a == 5}mine`", templateLit([lit("hey"), binExpr("==", ident("a"), lit(5)), lit("mine")]));
assertExpr("func`hey\\x`", taggedTemplate(ident("func"), template(["hey\\x"], [void 0])));
assertExpr("func`hey${4}\\x`", taggedTemplate(ident("func"), template(["hey","\\x"], ["hey",void 0], lit(4))));
assertExpr("`hey${`there${\"how\"}`}mine`", templateLit([lit("hey"),
           templateLit([lit("there"), lit("how"), lit("")]), lit("mine")]));
assertExpr("func`hey`", taggedTemplate(ident("func"), template(["hey"], ["hey"])));
assertExpr("func`hey${\"4\"}there`", taggedTemplate(ident("func"),
           template(["hey", "there"], ["hey", "there"], lit("4"))));
assertExpr("func`hey${\"4\"}there${5}`", taggedTemplate(ident("func"),
           template(["hey", "there", ""], ["hey", "there", ""],
                  lit("4"), lit(5))));
assertExpr("func`hey\r\n`", taggedTemplate(ident("func"), template(["hey\n"], ["hey\n"])));
assertExpr("func`hey${4}``${5}there``mine`",
           taggedTemplate(taggedTemplate(taggedTemplate(
               ident("func"), template(["hey", ""], ["hey", ""], lit(4))),
               template(["", "there"], ["", "there"], lit(5))),
               template(["mine"], ["mine"])));

// multi-line template string - line numbers
var node = Reflect.parse("`\n\n   ${2}\n\n\n`");
Pattern({loc:{start:{line:1, column:1}, end:{line:6, column:2}, source:null}, type:"Program",
body:[{loc:{start:{line:1, column:1}, end:{line:6, column:2}, source:null},
type:"ExpressionStatement", expression:{loc:{start:{line:1, column:1}, end:{line:6, column:2},
source:null}, type:"TemplateLiteral", elements:[{loc:{start:{line:1, column:1}, end:{line:3,
column:6}, source:null}, type:"Literal", value:"\n\n   "}, {loc:{start:{line:3, column:6},
end:{line:3, column:7}, source:null}, type:"Literal", value:2}, {loc:{start:{line:3, column:7},
end:{line:6, column:2}, source:null}, type:"Literal", value:"\n\n\n"}]}}]}).match(node);


assertStringExpr("\"hey there\"", literal("hey there"));

}

runtest(test);
