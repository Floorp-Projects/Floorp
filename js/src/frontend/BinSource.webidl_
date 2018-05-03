// Type aliases and enums.

typedef FrozenArray<(SpreadElement or Expression)> Arguments;
typedef DOMString string;
typedef string Identifier;
typedef string IdentifierName;
typedef string Label;

enum VariableDeclarationKind {
  "var",
  "let",
  "const"
};

enum CompoundAssignmentOperator {
  "+=",
  "-=",
  "*=",
  "/=",
  "%=",
  "**=",
  "<<=",
  ">>=",
  ">>>=",
  "|=",
  "^=",
  "&="
};

enum BinaryOperator {
  ",",
  "||",
  "&&",
  "|",
  "^",
  "&",
  "==",
  "!=",
  "===",
  "!==",
  "<",
  "<=",
  ">",
  ">=",
  "in",
  "instanceof",
  "<<",
  ">>",
  ">>>",
  "+",
  "-",
  "*",
  "/",
  "%",
  "**",
};

enum UnaryOperator {
  "+",
  "-",
  "!",
  "~",
  "typeof",
  "void",
  "delete"
};

enum UpdateOperator {
  "++",
  "--"
};


// deferred assertions

interface AssertedBlockScope {
  // checked eagerly during transformation
  attribute FrozenArray<IdentifierName> lexicallyDeclaredNames;

  // checked lazily as inner functions are invoked
  attribute FrozenArray<IdentifierName> capturedNames;
  attribute boolean hasDirectEval;
};

interface AssertedVarScope {
  // checked eagerly during transformation
  attribute FrozenArray<IdentifierName> lexicallyDeclaredNames;
  attribute FrozenArray<IdentifierName> varDeclaredNames;

  // checked lazily as inner functions are invoked
  attribute FrozenArray<IdentifierName> capturedNames;
  attribute boolean hasDirectEval;
};

interface AssertedParameterScope {
  // checked eagerly during transformation
  attribute FrozenArray<IdentifierName> parameterNames;

  // checked lazily as inner functions are invoked
  attribute FrozenArray<IdentifierName> capturedNames;
  attribute boolean hasDirectEval;
};

// nodes

interface Node {
  [TypeIndicator] readonly attribute Type type;
};

typedef (Script or Module) Program;

typedef (DoWhileStatement or
         ForInStatement or
         ForOfStatement or
         ForStatement or
         WhileStatement)
        IterationStatement;

typedef (Block or
         BreakStatement or
         ContinueStatement or
         ClassDeclaration or
         DebuggerStatement or
         EmptyStatement or
         ExpressionStatement or
         FunctionDeclaration or
         IfStatement or
         IterationStatement or
         LabelledStatement or
         ReturnStatement or
         SwitchStatement or
         SwitchStatementWithDefault or
         ThrowStatement or
         TryCatchStatement or
         TryFinallyStatement or
         VariableDeclaration or
         WithStatement)
        Statement;

typedef (LiteralBooleanExpression or
         LiteralInfinityExpression or
         LiteralNullExpression or
         LiteralNumericExpression or
         LiteralStringExpression)
        Literal;

typedef (Literal or
         LiteralRegExpExpression or
         ArrayExpression or
         ArrowExpression or
         AssignmentExpression or
         BinaryExpression or
         CallExpression or
         CompoundAssignmentExpression or
         ComputedMemberExpression or
         ConditionalExpression or
         ClassExpression or
         FunctionExpression or
         IdentifierExpression or
         NewExpression or
         NewTargetExpression or
         ObjectExpression or
         UnaryExpression or
         StaticMemberExpression or
         TemplateExpression or
         ThisExpression or
         UpdateExpression or
         YieldExpression or
         YieldStarExpression or
         AwaitExpression)
        Expression;

typedef (ComputedPropertyName or
         LiteralPropertyName)
        PropertyName;

typedef (Method or Getter or Setter) MethodDefinition;

typedef (MethodDefinition or
         DataProperty or
         ShorthandProperty)
        ObjectProperty;

typedef (ExportAllFrom or
         ExportFrom or
         ExportLocals or
         ExportDefault or
         Export)
        ExportDeclaration;

typedef (ImportNamespace or Import) ImportDeclaration;

typedef (EagerFunctionDeclaration or SkippableFunctionDeclaration) FunctionDeclaration;

typedef (EagerFunctionExpression or SkippableFunctionExpression) FunctionExpression;

typedef (EagerMethod or SkippableMethod) Method;

typedef (EagerGetter or SkippableGetter) Getter;

typedef (EagerSetter or SkippableSetter) Setter;

typedef (EagerArrowExpression or SkippableArrowExpression) ArrowExpression;

// bindings

interface BindingIdentifier : Node {
  attribute Identifier name;
};

typedef (ObjectBinding or
         ArrayBinding)
        BindingPattern;
typedef (BindingPattern or
         BindingIdentifier)
        Binding;

typedef (AssignmentTargetIdentifier or
         ComputedMemberAssignmentTarget or
         StaticMemberAssignmentTarget)
        SimpleAssignmentTarget;
typedef (ObjectAssignmentTarget or
         ArrayAssignmentTarget)
        AssignmentTargetPattern;
// `DestructuringAssignmentTarget`
typedef (AssignmentTargetPattern or
         SimpleAssignmentTarget)
        AssignmentTarget;

// `FormalParameter`
typedef (Binding or
         BindingWithInitializer)
        Parameter;

interface BindingWithInitializer : Node {
  attribute Binding binding;
  attribute Expression init;
};

interface AssignmentTargetIdentifier : Node {
  attribute Identifier name;
};

interface ComputedMemberAssignmentTarget : Node {
  // The object whose property is being assigned.
  attribute (Expression or Super) _object;
  // The expression resolving to the name of the property to be accessed.
  attribute Expression expression;
};

interface StaticMemberAssignmentTarget : Node {
  // The object whose property is being assigned.
  attribute (Expression or Super) _object;
  // The name of the property to be accessed.
  attribute IdentifierName property;
};

// `ArrayBindingPattern`
interface ArrayBinding : Node {
  // The elements of the array pattern; a null value represents an elision.
  attribute FrozenArray<(Binding or BindingWithInitializer)?> elements;
  attribute Binding? rest;
};

// `SingleNameBinding`
interface BindingPropertyIdentifier : Node {
  attribute BindingIdentifier binding;
  attribute Expression? init;
};

// `BindingProperty :: PropertyName : BindingElement`
interface BindingPropertyProperty : Node {
  attribute PropertyName name;
  attribute (Binding or BindingWithInitializer) binding;
};

typedef (BindingPropertyIdentifier or
         BindingPropertyProperty)
        BindingProperty;

interface ObjectBinding : Node {
  attribute FrozenArray<BindingProperty> properties;
};

// This interface represents the case where the initializer is present in
// `AssignmentElement :: DestructuringAssignmentTarget Initializer_opt`.
interface AssignmentTargetWithInitializer : Node {
  attribute AssignmentTarget binding;
  attribute Expression init;
};

// `ArrayAssignmentPattern`
interface ArrayAssignmentTarget : Node {
  // The elements of the array pattern; a null value represents an elision.
  attribute FrozenArray<(AssignmentTarget or AssignmentTargetWithInitializer?)> elements;
  attribute AssignmentTarget? rest;
};

// `AssignmentProperty :: IdentifierReference Initializer_opt`
interface AssignmentTargetPropertyIdentifier : Node {
  attribute AssignmentTargetIdentifier binding;
  attribute Expression? init;
};

// `AssignmentProperty :: PropertyName : Node`
interface AssignmentTargetPropertyProperty : Node {
  attribute PropertyName name;
  attribute (AssignmentTarget or AssignmentTargetWithInitializer) binding;
};

typedef (AssignmentTargetPropertyIdentifier or
         AssignmentTargetPropertyProperty)
        AssignmentTargetProperty;

// `ObjectAssignmentPattern`
interface ObjectAssignmentTarget : Node {
  attribute FrozenArray<AssignmentTargetProperty> properties;
};


// classes

interface ClassExpression : Node {
  attribute BindingIdentifier? name;
  attribute Expression? super;
  attribute FrozenArray<ClassElement> elements;
};

interface ClassDeclaration : Node {
  attribute BindingIdentifier name;
  attribute Expression? super;
  attribute FrozenArray<ClassElement> elements;
};

interface ClassElement : Node {
  // True iff `IsStatic` of ClassElement is true.
  attribute boolean isStatic;
  attribute MethodDefinition method;
};


// modules

interface Module : Node {
  attribute AssertedVarScope? scope;
  attribute FrozenArray<Directive> directives;
  attribute FrozenArray<(ImportDeclaration or ExportDeclaration or Statement)> items;
};

// An `ImportDeclaration` not including a namespace import.
interface Import : Node {
  attribute string moduleSpecifier;
  // `ImportedDefaultBinding`, if present.
  attribute BindingIdentifier? defaultBinding;
  attribute FrozenArray<ImportSpecifier> namedImports;
};

// An `ImportDeclaration` including a namespace import.
interface ImportNamespace : Node {
  attribute string moduleSpecifier;
  // `ImportedDefaultBinding`, if present.
  attribute BindingIdentifier? defaultBinding;
  attribute BindingIdentifier namespaceBinding;
};

interface ImportSpecifier : Node {
  // The `IdentifierName` in the production `ImportSpecifier :: IdentifierName as ImportedBinding`;
  // absent if this specifier represents the production `ImportSpecifier :: ImportedBinding`.
  attribute IdentifierName? name;
  attribute BindingIdentifier binding;
};

// `export * FromClause;`
interface ExportAllFrom : Node {
  attribute string moduleSpecifier;
};

// `export ExportClause FromClause;`
interface ExportFrom : Node {
  attribute FrozenArray<ExportFromSpecifier> namedExports;
  attribute string moduleSpecifier;
};

// `export ExportClause;`
interface ExportLocals : Node {
  attribute FrozenArray<ExportLocalSpecifier> namedExports;
};

// `export VariableStatement`, `export Declaration`
interface Export : Node {
  attribute (FunctionDeclaration or ClassDeclaration or VariableDeclaration) declaration;
};

// `export default HoistableDeclaration`,
// `export default ClassDeclaration`,
// `export default AssignmentExpression`
interface ExportDefault : Node {
  attribute (FunctionDeclaration or ClassDeclaration or Expression) body;
};

// `ExportSpecifier`, as part of an `ExportFrom`.
interface ExportFromSpecifier : Node {
  // The only `IdentifierName in `ExportSpecifier :: IdentifierName`,
  // or the first in `ExportSpecifier :: IdentifierName as IdentifierName`.
  attribute IdentifierName name;
  // The second `IdentifierName` in `ExportSpecifier :: IdentifierName as IdentifierName`,
  // if that is the production represented.
  attribute IdentifierName? exportedName;
};

// `ExportSpecifier`, as part of an `ExportLocals`.
interface ExportLocalSpecifier : Node {
  // The only `IdentifierName in `ExportSpecifier :: IdentifierName`,
  // or the first in `ExportSpecifier :: IdentifierName as IdentifierName`.
  attribute IdentifierExpression name;
  // The second `IdentifierName` in `ExportSpecifier :: IdentifierName as IdentifierName`, if present.
  attribute IdentifierName? exportedName;
};


// property definition

// `MethodDefinition :: PropertyName ( UniqueFormalParameters ) { FunctionBody }`,
// `GeneratorMethod :: * PropertyName ( UniqueFormalParameters ) { GeneratorBody }`,
// `AsyncMethod :: async PropertyName ( UniqueFormalParameters ) { AsyncFunctionBody }`
interface EagerMethod : Node {
  // True for `AsyncMethod`, false otherwise.
  attribute boolean isAsync;
  // True for `GeneratorMethod`, false otherwise.
  attribute boolean isGenerator;
  attribute PropertyName name;
  attribute AssertedParameterScope? parameterScope;
  attribute AssertedVarScope? bodyScope;
  // The `UniqueFormalParameters`.
  attribute FormalParameters params;
  attribute FunctionBody body;
};

[Skippable] interface SkippableMethod : Node {
  attribute EagerMethod skipped;
};

// `get PropertyName ( ) { FunctionBody }`
interface EagerGetter : Node {
  attribute PropertyName name;
  attribute AssertedVarScope? bodyScope;
  attribute FunctionBody body;
};

[Skippable] interface SkippableGetter : Node {
  attribute EagerGetter skipped;
};

// `set PropertyName ( PropertySetParameterList ) { FunctionBody }`
interface EagerSetter : Node {
  attribute PropertyName name;
  attribute AssertedParameterScope? parameterScope;
  attribute AssertedVarScope? bodyScope;
  // The `PropertySetParameterList`.
  attribute Parameter param;
  attribute FunctionBody body;
};

[Skippable] interface SkippableSetter : Node {
  attribute EagerSetter skipped;
};

// `PropertyDefinition :: PropertyName : AssignmentExpression`
interface DataProperty : Node {
  attribute PropertyName name;
  // The `AssignmentExpression`.
  attribute Expression expression;
};

// `PropertyDefinition :: IdentifierReference`
interface ShorthandProperty : Node {
  // The `IdentifierReference`.
  attribute IdentifierExpression name;
};

interface ComputedPropertyName : Node {
  attribute Expression expression;
};

// `LiteralPropertyName`
interface LiteralPropertyName : Node {
  attribute string value;
};


// literals

// `BooleanLiteral`
interface LiteralBooleanExpression : Node {
  attribute boolean value;
};

// A `NumericLiteral` for which the Number value of its MV is positive infinity.
interface LiteralInfinityExpression : Node { };

// `NullLiteral`
interface LiteralNullExpression : Node { };

// `NumericLiteral`
interface LiteralNumericExpression : Node {
  attribute double value;
};

// `RegularExpressionLiteral`
interface LiteralRegExpExpression : Node {
  attribute string pattern;
  attribute string flags;
};

// `StringLiteral`
interface LiteralStringExpression : Node {
  attribute string value;
};


// other expressions

// `ArrayLiteral`
interface ArrayExpression : Node {
  // The elements of the array literal; a null value represents an elision.
  attribute FrozenArray<(SpreadElement or Expression)?> elements;
};

// `ArrowFunction`,
// `AsyncArrowFunction`
interface EagerArrowExpression : Node {
  // True for `AsyncArrowFunction`, false otherwise.
  attribute boolean isAsync;
  attribute AssertedParameterScope? parameterScope;
  attribute AssertedVarScope? bodyScope;
  attribute FormalParameters params;
  attribute (FunctionBody or Expression) body;
};

[Skippable] interface SkippableArrowExpression : Node {
  attribute EagerArrowExpression skipped;
};

// `AssignmentExpression :: LeftHandSideExpression = AssignmentExpression`
interface AssignmentExpression : Node {
  // The `LeftHandSideExpression`.
  attribute AssignmentTarget binding;
  // The `AssignmentExpression` following the `=`.
  attribute Expression expression;
};

// `ExponentiationExpression`,
// `MultiplicativeExpression`,
// `AdditiveExpression`,
// `ShiftExpression`,
// `RelationalExpression`,
// `EqualityExpression`,
// `BitwiseANDExpression`,
// `BitwiseXORExpression`,
// `BitwiseORExpression`,
// `LogicalANDExpression`,
// `LogicalORExpression`
interface BinaryExpression : Node {
  attribute BinaryOperator operator;
  // The expression before the operator.
  attribute Expression left;
  // The expression after the operator.
  attribute Expression right;
};

interface CallExpression : Node {
  attribute (Expression or Super) callee;
  attribute Arguments arguments;
};

// `AssignmentExpression :: LeftHandSideExpression AssignmentOperator AssignmentExpression`
interface CompoundAssignmentExpression : Node {
  attribute CompoundAssignmentOperator operator;
  // The `LeftHandSideExpression`.
  attribute SimpleAssignmentTarget binding;
  // The `AssignmentExpression`.
  attribute Expression expression;
};

interface ComputedMemberExpression : Node {
  // The object whose property is being accessed.
  attribute (Expression or Super) _object;
  // The expression resolving to the name of the property to be accessed.
  attribute Expression expression;
};

// `ConditionalExpression :: LogicalORExpression ? AssignmentExpression : AssignmentExpression`
interface ConditionalExpression : Node {
  // The `LogicalORExpression`.
  attribute Expression test;
  // The first `AssignmentExpression`.
  attribute Expression consequent;
  // The second `AssignmentExpression`.
  attribute Expression alternate;
};

// `FunctionExpression`,
// `GeneratorExpression`,
// `AsyncFunctionExpression`,
interface EagerFunctionExpression : Node {
  attribute boolean isAsync;
  attribute boolean isGenerator;
  attribute BindingIdentifier? name;
  attribute AssertedParameterScope? parameterScope;
  attribute AssertedVarScope? bodyScope;
  attribute FormalParameters params;
  attribute FunctionBody body;
};

[Skippable] interface SkippableFunctionExpression : Node {
  attribute EagerFunctionExpression skipped;
};

// `IdentifierReference`
interface IdentifierExpression : Node {
  attribute Identifier name;
};

interface NewExpression : Node {
  attribute Expression callee;
  attribute Arguments arguments;
};

interface NewTargetExpression : Node { };

interface ObjectExpression : Node {
  attribute FrozenArray<ObjectProperty> properties;
};

interface UnaryExpression : Node {
  attribute UnaryOperator operator;
  attribute Expression operand;
};

interface StaticMemberExpression : Node {
  // The object whose property is being accessed.
  attribute (Expression or Super) _object;
  // The name of the property to be accessed.
  attribute IdentifierName property;
};

// `TemplateLiteral`,
// `MemberExpression :: MemberExpression TemplateLiteral`,
// `CallExpression : CallExpression TemplateLiteral`
interface TemplateExpression : Node {
  // The second `MemberExpression` or `CallExpression`, if present.
  attribute Expression? tag;
  // The contents of the template. This list must be alternating
  // TemplateElements and Expressions, beginning and ending with
  // TemplateElement.
  attribute FrozenArray<(Expression or TemplateElement)> elements;
};

// `PrimaryExpression :: this`
interface ThisExpression : Node { };

// `UpdateExpression :: LeftHandSideExpression ++`,
// `UpdateExpression :: LeftHandSideExpression --`,
// `UpdateExpression :: ++ LeftHandSideExpression`,
// `UpdateExpression :: -- LeftHandSideExpression`
interface UpdateExpression : Node {
  // True for `UpdateExpression :: ++ LeftHandSideExpression` and
  // `UpdateExpression :: -- LeftHandSideExpression`, false otherwise.
  attribute boolean isPrefix;
  attribute UpdateOperator operator;
  attribute SimpleAssignmentTarget operand;
};

// `YieldExpression :: yield`,
// `YieldExpression :: yield AssignmentExpression`
interface YieldExpression : Node {
  // The `AssignmentExpression`, if present.
  attribute Expression? expression;
};

// `YieldExpression :: yield * AssignmentExpression`
interface YieldStarExpression : Node {
  attribute Expression expression;
};

interface AwaitExpression : Node {
  attribute Expression expression;
};


// other statements

interface BreakStatement : Node {
  attribute Label? label;
};

interface ContinueStatement : Node {
  attribute Label? label;
};

interface DebuggerStatement : Node { };

interface DoWhileStatement : Node {
  attribute Expression test;
  attribute Statement body;
};

interface EmptyStatement : Node { };

interface ExpressionStatement : Node {
  attribute Expression expression;
};

interface ForInOfBinding : Node {
  attribute VariableDeclarationKind kind;
  attribute Binding binding;
};

// `for ( LeftHandSideExpression in Expression ) Statement`,
// `for ( var ForBinding in Expression ) Statement`,
// `for ( ForDeclaration in Expression ) Statement`,
// `for ( var BindingIdentifier Initializer in Expression ) Statement`
interface ForInStatement : Node {
  // The expression or declaration before `in`.
  attribute (ForInOfBinding or AssignmentTarget) left;
  // The expression after `in`.
  attribute Expression right;
  attribute Statement body;
};

// `for ( LeftHandSideExpression of Expression ) Statement`,
// `for ( var ForBinding of Expression ) Statement`,
// `for ( ForDeclaration of Expression ) Statement`
interface ForOfStatement : Node {
  // The expression or declaration before `of`.
  attribute (ForInOfBinding or AssignmentTarget) left;
  // The expression after `of`.
  attribute Expression right;
  attribute Statement body;
};

// `for ( Expression ; Expression ; Expression ) Statement`,
// `for ( var VariableDeclarationList ; Expression ; Expression ) Statement`
interface ForStatement : Node {
  // The expression or declaration before the first `;`, if present.
  attribute (VariableDeclaration or Expression)? init;
  // The expression before the second `;`, if present
  attribute Expression? test;
  // The expression after the second `;`, if present
  attribute Expression? update;
  attribute Statement body;
};

// `if ( Expression ) Statement`,
// `if ( Expression ) Statement else Statement`,
interface IfStatement : Node {
  attribute Expression test;
  // The first `Statement`.
  attribute Statement consequent;
  // The second `Statement`, if present.
  attribute Statement? alternate;
};

interface LabelledStatement : Node {
  attribute Label label;
  attribute Statement body;
};

interface ReturnStatement : Node {
  attribute Expression? expression;
};

// A `SwitchStatement` whose `CaseBlock` is
//   `CaseBlock :: { CaseClauses }`.
interface SwitchStatement : Node {
  attribute Expression discriminant;
  attribute FrozenArray<SwitchCase> cases;
};

// A `SwitchStatement` whose `CaseBlock` is
//   `CaseBlock :: { CaseClauses DefaultClause CaseClauses }`.
interface SwitchStatementWithDefault : Node {
  attribute Expression discriminant;
  // The `CaseClauses` before the `DefaultClause`.
  attribute FrozenArray<SwitchCase> preDefaultCases;
  // The `DefaultClause`.
  attribute SwitchDefault defaultCase;
  // The `CaseClauses` after the `DefaultClause`.
  attribute FrozenArray<SwitchCase> postDefaultCases;
};

interface ThrowStatement : Node {
  attribute Expression expression;
};

// `TryStatement :: try Block Catch`
interface TryCatchStatement : Node {
  attribute Block body;
  attribute CatchClause catchClause;
};

// `TryStatement :: try Block Finally`,
// `TryStatement :: try Block Catch Finally`
interface TryFinallyStatement : Node {
  // The `Block`.
  attribute Block body;
  // The `Catch`, if present.
  attribute CatchClause? catchClause;
  // The `Finally`.
  attribute Block finalizer;
};

interface WhileStatement : Node {
  attribute Expression test;
  attribute Statement body;
};

interface WithStatement : Node {
  attribute Expression _object;
  attribute Statement body;
};


// other nodes

interface Block : Node {
  attribute AssertedBlockScope? scope;
  attribute FrozenArray<Statement> statements;
};

// `Catch`
interface CatchClause : Node {
  // `AssertedParameterScope` is used for catch bindings so the declared names
  // are checked using BoundNames.
  attribute AssertedParameterScope? bindingScope;
  attribute Binding binding;
  attribute Block body;
};

// An item in a `DirectivePrologue`
interface Directive : Node {
  attribute string rawValue;
};

interface FormalParameters : Node {
  attribute FrozenArray<Parameter> items;
  attribute Binding? rest;
};

interface FunctionBody : Node {
  attribute FrozenArray<Directive> directives;
  attribute FrozenArray<Statement> statements;
};



// `FunctionDeclaration`,
// `GeneratorDeclaration`,
// `AsyncFunctionDeclaration`
interface EagerFunctionDeclaration : Node {
  attribute boolean isAsync;
  attribute boolean isGenerator;
  attribute BindingIdentifier name;
  attribute AssertedParameterScope? parameterScope;
  attribute AssertedVarScope? bodyScope;
  attribute FormalParameters params;
  attribute FunctionBody body;
};

[Skippable] interface SkippableFunctionDeclaration : Node {
  attribute EagerFunctionDeclaration skipped;
};

interface Script : Node {
  attribute AssertedVarScope? scope;
  attribute FrozenArray<Directive> directives;
  attribute FrozenArray<Statement> statements;
};

interface SpreadElement : Node {
  attribute Expression expression;
};

// `super`
interface Super : Node { };

// `CaseClause`
interface SwitchCase : Node {
  attribute Expression test;
  attribute FrozenArray<Statement> consequent;
};

// `DefaultClause`
interface SwitchDefault : Node {
  attribute FrozenArray<Statement> consequent;
};

// `TemplateCharacters`
interface TemplateElement : Node {
  attribute string rawValue;
};

interface VariableDeclaration : Node {
  attribute VariableDeclarationKind kind;
  [NonEmpty] attribute FrozenArray<VariableDeclarator> declarators;
};

interface VariableDeclarator : Node {
  attribute Binding binding;
  attribute Expression? init;
};
