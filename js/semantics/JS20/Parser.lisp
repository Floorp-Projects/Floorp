;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(defparameter *jw-source* 
  '((line-grammar code-grammar :lr-1 :program)
    
    (%section "Expressions")
    (grammar-argument :beta allow-in no-in)
    
    (%subsection "Identifiers")
    (production :identifier ($identifier) identifier-identifier)
    (production :identifier (get) identifier-get)
    (production :identifier (set) identifier-set)
    ;(production :identifier (namespace) identifier-namespace)
    ;(production :identifier (use) identifier-use)
    
    (production :qualified-identifier (:identifier) qualified-identifier-identifier)
    (production :qualified-identifier (:qualifier \:\: :qualified-identifier) qualified-identifier-qualifier)
    
    (production :qualifier (:identifier) qualifier-identifier)
    (production :qualifier (public) qualifier-public)
    (production :qualifier (package) qualifier-package)
    (production :qualifier (private) qualifier-private)
    (production :qualifier (super) qualifier-super)
    ;(production :qualifier (:parenthesized-expression) qualifier-parenthesized-expression)
    
    
    (%subsection "Primary Expressions")
    (production :primary-expression (null) primary-expression-null)
    (production :primary-expression (true) primary-expression-true)
    (production :primary-expression (false) primary-expression-false)
    (production :primary-expression (public) primary-expression-public)
    (production :primary-expression ($number) primary-expression-number)
    (production :primary-expression ($number :no-line-break $string) primary-expression-number-with-unit)
    (production :primary-expression ($string) primary-expression-string)
    (production :primary-expression (this) primary-expression-this)
    (production :primary-expression ($regular-expression) primary-expression-regular-expression)
    (production :primary-expression (:parenthesized-expression) primary-expression-parenthesized-expression)
    (production :primary-expression (:parenthesized-expression :no-line-break $string) primary-expression-parenthesized-expression-with-unit)
    (production :primary-expression (:array-literal) primary-expression-array-literal)
    (production :primary-expression (:object-literal) primary-expression-object-literal)
    (production :primary-expression (:function-expression) primary-expression-function-expression)
    (production :primary-expression (eval \( (:assignment-expression allow-in) \)) primary-expression-eval)
    
    (production :parenthesized-expression (\( (:list-expression allow-in) \)) parenthesized-expression-list-expression)
    
    
    (%subsection "Function Expressions")
    (production :function-expression (function :function-signature :block) function-expression-anonymous)
    (production :function-expression (function :identifier :function-signature :block) function-expression-named)
    
    
    (%subsection "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression)
    
    (production :field-name (:qualified-identifier) field-name-identifier)
    (production :field-name ($string) field-name-string)
    (production :field-name ($number) field-name-number)
    
    
    (%subsection "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    
    
    (%subsection "Postfix Unary Operators")
    (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression)
    (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression)
    (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression)
    
    (production :attribute-expression (:qualified-identifier) attribute-expression-qualified-identifier)
    (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator)
    (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call1)
    
    (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression)
    (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression)
    (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator)
    (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call)
    (production :full-postfix-expression (:postfix-expression :no-line-break ++) full-postfix-expression-increment)
    (production :full-postfix-expression (:postfix-expression :no-line-break --) full-postfix-expression-decrement)
    (? js2
      (production :full-postfix-expression (:postfix-expression @ :qualified-identifier) full-postfix-expression-coerce)
      (production :full-postfix-expression (:postfix-expression @ :parenthesized-expression) full-postfix-expression-indirect-coerce))
    
    (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new)
    
    (production :short-new-expression (new :short-new-subexpression) short-new-expression-new)
    
    (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression)
    (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier)
    (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator)
    (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression)
    
    (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full)
    (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short)
    
    (production :member-operator ([ :argument-list ]) member-operator-array)
    (production :member-operator (\. :qualified-identifier) member-operator-property)
    (production :member-operator (\. :parenthesized-expression) member-operator-indirect-property)
    
    (production :arguments (\( :argument-list \)) arguments-argument-list)
    
    (production :argument-list () argument-list-none)
    (production :argument-list ((:list-expression allow-in)) argument-list-unnamed)
    (production :argument-list (:named-argument-list) argument-list-named)
    
    (production :named-argument-list (:literal-field) named-argument-list-one)
    (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed)
    (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more)
    
    
    (%subsection "Prefix Unary Operators")
    (production :unary-expression (:postfix-expression) unary-expression-postfix)
    (production :unary-expression (delete :postfix-expression) unary-expression-delete)
    (production :unary-expression (typeof :unary-expression) unary-expression-typeof)
    (production :unary-expression (++ :postfix-expression) unary-expression-increment)
    (production :unary-expression (-- :postfix-expression) unary-expression-decrement)
    (production :unary-expression (+ :unary-expression) unary-expression-plus)
    (production :unary-expression (- :unary-expression) unary-expression-minus)
    (production :unary-expression (~ :unary-expression) unary-expression-bitwise-not)
    (production :unary-expression (! :unary-expression) unary-expression-logical-not)
    ;(production :unary-expression (? :unary-expression) unary-expression-question)
    
    
    (%subsection "Multiplicative Operators")
    (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary)
    (production :multiplicative-expression (:multiplicative-expression * :unary-expression) multiplicative-expression-multiply)
    (production :multiplicative-expression (:multiplicative-expression / :unary-expression) multiplicative-expression-divide)
    (production :multiplicative-expression (:multiplicative-expression % :unary-expression) multiplicative-expression-remainder)
    
    
    (%subsection "Additive Operators")
    (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative)
    (production :additive-expression (:additive-expression + :multiplicative-expression) additive-expression-add)
    (production :additive-expression (:additive-expression - :multiplicative-expression) additive-expression-subtract)
    
    
    (%subsection "Bitwise Shift Operators")
    (production :shift-expression (:additive-expression) shift-expression-additive)
    (production :shift-expression (:shift-expression << :additive-expression) shift-expression-left)
    (production :shift-expression (:shift-expression >> :additive-expression) shift-expression-right-signed)
    (production :shift-expression (:shift-expression >>> :additive-expression) shift-expression-right-unsigned)
    
    
    (%subsection "Relational Operators")
    (production (:relational-expression :beta) (:shift-expression) relational-expression-shift)
    (production (:relational-expression :beta) ((:relational-expression :beta) < :shift-expression) relational-expression-less)
    (production (:relational-expression :beta) ((:relational-expression :beta) > :shift-expression) relational-expression-greater)
    (production (:relational-expression :beta) ((:relational-expression :beta) <= :shift-expression) relational-expression-less-or-equal)
    (production (:relational-expression :beta) ((:relational-expression :beta) >= :shift-expression) relational-expression-greater-or-equal)
    (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof)
    (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression) relational-expression-in)
    
    
    (%subsection "Equality Operators")
    (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational)
    (production (:equality-expression :beta) ((:equality-expression :beta) == (:relational-expression :beta)) equality-expression-equal)
    (production (:equality-expression :beta) ((:equality-expression :beta) != (:relational-expression :beta)) equality-expression-not-equal)
    (production (:equality-expression :beta) ((:equality-expression :beta) === (:relational-expression :beta)) equality-expression-strict-equal)
    (production (:equality-expression :beta) ((:equality-expression :beta) !== (:relational-expression :beta)) equality-expression-strict-not-equal)
    
    
    (%subsection "Binary Bitwise Operators")
    (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality)
    (production (:bitwise-and-expression :beta) ((:bitwise-and-expression :beta) & (:equality-expression :beta)) bitwise-and-expression-and)
    
    (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and)
    (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ (:bitwise-and-expression :beta)) bitwise-xor-expression-xor)
    
    (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor)
    (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| (:bitwise-xor-expression :beta)) bitwise-or-expression-or)
    
    
    (%subsection "Binary Logical Operators")
    (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or)
    (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and)
    
    (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and)
    (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor)
    
    (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor)
    (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or)
    
    
    (%subsection "Conditional Operator")
    (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or)
    (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional)
    
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or)
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional)
    
    
    (%subsection "Assignment Operators")
    (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional)
    (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment)
    (production (:assignment-expression :beta) (:postfix-expression :compound-assignment (:assignment-expression :beta)) assignment-expression-compound)
    
    (production :compound-assignment (*=) compound-assignment-multiply)
    (production :compound-assignment (/=) compound-assignment-divide)
    (production :compound-assignment (%=) compound-assignment-remainder)
    (production :compound-assignment (+=) compound-assignment-add)
    (production :compound-assignment (-=) compound-assignment-subtract)
    (production :compound-assignment (<<=) compound-assignment-shift-left)
    (production :compound-assignment (>>=) compound-assignment-shift-right)
    (production :compound-assignment (>>>=) compound-assignment-shift-right-unsigned)
    (production :compound-assignment (&=) compound-assignment-bitwise-and)
    (production :compound-assignment (^=) compound-assignment-bitwise-xor)
    (production :compound-assignment (\|=) compound-assignment-bitwise-or)
    (production :compound-assignment (&&=) compound-assignment-logical-and)
    (production :compound-assignment (^^=) compound-assignment-logical-xor)
    (production :compound-assignment (\|\|=) compound-assignment-logical-or)
    
    
    (%subsection "Comma Expressions")
    (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment)
    (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma)
    
    (production :optional-expression ((:list-expression allow-in)) optional-expression-expression)
    (production :optional-expression () optional-expression-empty)
    
    
    (%subsection "Type Expressions")
    (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression)
    
    
    (%section "Statements")
    
    (grammar-argument :omega
                      abbrev              ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      abbrev-no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)               ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (production (:top-statement :omega_2) ((:statement :omega_2)) top-statement-statement)
    ;(production (:top-statement :omega_2) (:attribute-definition (:noninsertable-semicolon :omega_2)) top-statement-attribute-definition)
    (production (:top-statement :omega_2) (:language-declaration (:noninsertable-semicolon :omega_2)) top-statement-language-declaration)
    (production (:top-statement :omega_2) (:package-definition) top-statement-package-definition)
    
    (production (:statement :omega) ((:annotated-definition :omega)) statement-annotated-definition)
    (production (:statement :omega) (:empty-statement) statement-empty-statement)
    (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement)
    (production (:statement :omega) (:annotated-block) statement-annotated-block)
    (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement)
    (production (:statement :omega) ((:if-statement :omega)) statement-if-statement)
    (production (:statement :omega) (:switch-statement) statement-switch-statement)
    (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement)
    (production (:statement :omega) ((:while-statement :omega)) statement-while-statement)
    (production (:statement :omega) ((:for-statement :omega)) statement-for-statement)
    (production (:statement :omega) ((:with-statement :omega)) statement-with-statement)
    (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement)
    (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement)
    (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement)
    (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement)
    (production (:statement :omega) (:try-statement) statement-try-statement)
    (production (:statement :omega) (:import-statement (:semicolon :omega)) statement-import-statement)
    (production (:statement :omega) (:use-statement (:semicolon :omega)) statement-use-statement)
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
    
    (production (:noninsertable-semicolon :omega_2) (\;) noninsertable-semicolon-semicolon)
    (production (:noninsertable-semicolon abbrev) () noninsertable-semicolon-abbrev)
    
    
    (%subsection "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%subsection "Expression Statement")
    (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression)
    
    
    (%subsection "Block")
    (production :annotated-block (:block) annotated-block-block)
    (production :annotated-block (#|(:- package)|# :attributes :no-line-break :block) annotated-block-attributes-and-block)
    ;(production :annotated-block (package :no-line-break :block) annotated-block-package-and-block)
    
    (production :block ({ :top-statements }) block-top-statements)
    
    (production :top-statements () top-statements-none)
    (production :top-statements (:top-statements-prefix (:top-statement abbrev)) top-statements-more)
    
    (production :top-statements-prefix () top-statements-prefix-none)
    (production :top-statements-prefix (:top-statements-prefix (:top-statement full)) top-statements-prefix-more)
    
    
    (%subsection "Labeled Statements")
    (production (:labeled-statement :omega) (:identifier \: (:statement :omega)) labeled-statement-label)
    
    
    (%subsection "If Statement")
    (production (:if-statement abbrev) (if :parenthesized-expression (:statement abbrev)) if-statement-if-then-abbrev)
    (production (:if-statement full) (if :parenthesized-expression (:statement full)) if-statement-if-then-full)
    (production (:if-statement :omega) (if :parenthesized-expression (:statement abbrev-no-short-if)
                                           else (:statement :omega)) if-statement-if-then-else)
    
    
    (%subsection "Switch Statement")
    (production :switch-statement (switch :parenthesized-expression { :case-statements }) switch-statement-cases)
    
    (production (:case-statement :omega_2) ((:statement :omega_2)) case-statement-statement)
    (production (:case-statement :omega_2) (:case-label) case-statement-case-label)
    
    (production :case-label (case (:list-expression allow-in) \:) case-label-case)
    (production :case-label (default \:) case-label-default)
    
    (production :case-statements () case-statements-none)
    (production :case-statements (:case-label) case-statements-one)
    (production :case-statements (:case-label :case-statements-prefix (:case-statement abbrev)) case-statements-more)
    
    (production :case-statements-prefix () case-statements-prefix-none)
    (production :case-statements-prefix (:case-statements-prefix (:case-statement full)) case-statements-prefix-more)
    
    
    (%subsection "Do-While Statement")
    (production :do-statement (do (:statement abbrev) while :parenthesized-expression) do-statement-do-while)
    
    
    (%subsection "While Statement")
    (production (:while-statement :omega) (while :parenthesized-expression (:statement :omega)) while-statement-while)
    
    
    (%subsection "For Statements")
    (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                             (:statement :omega)) for-statement-c-style)
    (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:statement :omega)) for-statement-in)
    
    (production :for-initializer () for-initializer-empty)
    (production :for-initializer ((:list-expression no-in)) for-initializer-expression)
    (production :for-initializer (:attributes :no-line-break :variable-definition-kind (:variable-binding-list no-in)) for-initializer-variable-definition)
    
    (production :for-in-binding (:postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:attributes :no-line-break :variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    
    
    (%subsection "With Statement")
    (production (:with-statement :omega) (with :parenthesized-expression (:statement :omega)) with-statement-with)
    
    
    (%subsection "Continue and Break Statements")
    (production :continue-statement (continue) continue-statement-unlabeled)
    (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled)
    
    (production :break-statement (break) break-statement-unlabeled)
    (production :break-statement (break :no-line-break :identifier) break-statement-labeled)
    
    
    (%subsection "Return Statement")
    (production :return-statement (return) return-statement-default)
    (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression)
    
    
    (%subsection "Throw Statement")
    (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw)
    
    
    (%subsection "Try Statement")
    (production :try-statement (try :annotated-block :catch-clauses) try-statement-catch-clauses)
    (production :try-statement (try :annotated-block :finally-clause) try-statement-finally-clause)
    (production :try-statement (try :annotated-block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
    
    (production :catch-clauses (:catch-clause) catch-clauses-one)
    (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
    
    (production :catch-clause (catch \( :typed-identifier \) :annotated-block) catch-clause-block)
    
    (production :finally-clause (finally :annotated-block) finally-clause-block)
    
    
    (%subsection "Import Statement")
    (production :import-statement (import :import-list) import-statement-import)
    (production :import-statement (use :no-line-break import :import-list) import-statement-use-import)
    
    (production :import-list (:import-binding) import-list-one)
    (production :import-list (:import-list \, :import-binding) import-list-more)
    
    (production :import-binding (:package-name) import-binding-package-name)
    (production :import-binding (:identifier = :package-name) import-binding-named-package-name)
    
    (production :package-name ($string) package-name-string)
    ;(production :package-name (:compound-package-name) package-name-compound)
    
    ;(production :compound-package-name (:identifier) compound-package-name-one)
    ;(production :compound-package-name (:compound-package-name \. :identifier) compound-package-name-more)
    
    
    (%subsection "Use Statement")
    (production :use-statement (use :no-line-break namespace :nonassignment-expression-list) use-statement-normal)
    
    (production :nonassignment-expression-list ((:non-assignment-expression allow-in)) nonassignment-expression-list-one)
    (production :nonassignment-expression-list (:nonassignment-expression-list \, (:non-assignment-expression allow-in)) nonassignment-expression-list-more)
    
    
    (%section "Definitions")
    (production (:annotated-definition :omega) ((:definition :omega)) annotated-definition-definition)
    (production (:annotated-definition :omega) (#|(:- package)|# :attributes :no-line-break (:definition :omega)) annotated-definition-attribute-and-definition)
    ;(production (:annotated-definition :omega) (package :no-line-break (:annotated-definition :omega)) annotated-definition-package-and-definition)
    
    #|
       (%text :grammar
         "The last two " (:grammar-symbol (:annotated-definition :omega)) " productions together have the same effect as "
         (:grammar-symbol (:annotated-definition :omega)) " " :derives-10 " " (:grammar-symbol :attributes) " " (:grammar-symbol (:definition :omega))
         " except that the latter would make the grammar non-LR(1).")
       |#
    
    (production :attributes (:attribute) attributes-one)
    (production :attributes (:attribute :no-line-break :attributes) attributes-more)
    
    (production :attribute (:attribute-expression) attribute-attribute-expression)
    (production :attribute (:keyword-attribute) attribute-keyword-attribute)
    
    (production :keyword-attribute (final) keyword-attribute-final)
    (production :keyword-attribute (package) keyword-attribute-package)
    (production :keyword-attribute (private) keyword-attribute-private)
    (production :keyword-attribute (public) keyword-attribute-public)
    (production :keyword-attribute (static) keyword-attribute-static)
    (production :keyword-attribute (volatile) keyword-attribute-volatile)
    
    (production (:definition :omega) (:export-definition (:semicolon :omega)) definition-export-definition)
    (production (:definition :omega) (:variable-definition (:semicolon :omega)) definition-variable-definition)
    (production (:definition :omega) ((:function-definition :omega)) definition-function-definition)
    (production (:definition :omega) ((:class-definition :omega)) definition-class-definition)
    (production (:definition :omega) ((:interface-definition :omega)) definition-interface-definition)
    (production (:definition :omega) (:namespace-definition (:semicolon :omega)) definition-namespace-definition)
    
    
    (%subsection "Export Definition")
    (production :export-definition (export :export-binding-list) export-definition-definition)
    
    (production :export-binding-list (:export-binding) export-binding-list-one)
    (production :export-binding-list (:export-binding-list \, :export-binding) export-binding-list-more)
    
    (production :export-binding (:function-name) export-binding-simple)
    (production :export-binding (:function-name = :function-name) export-binding-initializer)
    
    
    (%subsection "Variable Definition")
    (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition)
    
    (production :variable-definition-kind (var) variable-definition-kind-var)
    (production :variable-definition-kind (const) variable-definition-kind-const)
    
    (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one)
    (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more)
    
    (production (:variable-binding :beta) ((:typed-variable :beta)) variable-binding-simple)
    (production (:variable-binding :beta) ((:typed-variable :beta) = (:assignment-expression :beta)) variable-binding-initialized)
    (production (:variable-binding :beta) ((:typed-variable :beta) = :attribute :no-line-break :attributes) variable-binding-attributes)
    
    (production (:typed-variable :beta) (:qualified-identifier) typed-variable-identifier)
    (production (:typed-variable :beta) (:qualified-identifier \: (:type-expression :beta)) typed-variable-identifier-and-type)
    ;(production (:typed-variable :beta) ((:type-expression :beta) :qualified-identifier) typed-variable-type-and-identifier)
    
    
    (%subsection "Function Definition")
    (production (:function-definition :omega) (:function-declaration :block) function-definition-definition)
    (production (:function-definition :omega) (:function-declaration (:semicolon :omega)) function-definition-declaration)
    
    (production :function-declaration (function :function-name :function-signature) function-declaration-signature-and-body)
    (production :function-declaration (constructor :no-line-break :identifier :function-signature) function-declaration-constructor)
    
    (production :function-name (:qualified-identifier) function-name-function)
    (production :function-name (get :no-line-break (:- \() :qualified-identifier) function-name-getter)
    (production :function-name (set :no-line-break (:- \() :qualified-identifier) function-name-setter)
    
    (production :function-signature (:parameter-signature :result-signature) function-signature-parameter-and-result-signatures)
    
    (production :parameter-signature (\( :parameters \)) parameter-signature-parameters)
    
    (production :parameters () parameters-none)
    (production :parameters (:rest-parameter) parameters-rest)
    (production :parameters (:required-parameters) parameters-required-parameters)
    (production :parameters (:optional-parameters) parameters-optional-parameters)
    (production :parameters (:required-parameters \, :rest-parameter) parameters-required-and-rest)
    (production :parameters (:optional-parameters \, :rest-parameter) parameters-optional-and-rest)
    
    (production :required-parameters (:required-parameter) required-parameters-one)
    (production :required-parameters (:required-parameters \, :required-parameter) required-parameters-more)
    
    (production :optional-parameters (:optional-parameter) optional-parameters-one)
    (production :optional-parameters (:required-parameters \, :optional-parameter) optional-parameters-required-more)
    (production :optional-parameters (:optional-parameters \, :optional-parameter) optional-parameters-optional-more)
    
    (production :required-parameter (:typed-identifier) required-parameter-typed-identifier)
    
    (production :optional-parameter (:typed-identifier = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
    
    (production :typed-identifier (:identifier) typed-identifier-identifier)
    (production :typed-identifier (:identifier \: (:type-expression allow-in)) typed-identifier-identifier-and-type)
    ;(production :typed-identifier ((:type-expression allow-in) :identifier) typed-identifier-type-and-identifier)
    
    (production :rest-parameter (\.\.\.) rest-parameter-none)
    (production :rest-parameter (\.\.\. :typed-identifier) rest-parameter-typed-identifier)
    (production :rest-parameter (\.\.\. :typed-identifier = (:assignment-expression allow-in)) rest-parameter-assignment-expression)
    
    (production :result-signature () result-signature-none)
    (production :result-signature (\: (:type-expression allow-in)) result-signature-colon-and-type-expression)
    ;(production :result-signature ((:- {) (:type-expression allow-in)) result-signature-type-expression)
    
    
    (%subsection "Class Definition")
    (production (:class-definition :omega) (class :qualified-identifier :superclass :implements-list :block) class-definition-definition)
    (production (:class-definition :omega) (class :qualified-identifier (:semicolon :omega)) class-definition-declaration)
    
    (production :superclass () superclass-none)
    (production :superclass (extends (:type-expression allow-in)) superclass-one)
    
    (production :implements-list () implements-list-none)
    (production :implements-list (implements :type-expression-list) implements-list-one)
    
    (production :type-expression-list ((:type-expression allow-in)) type-expression-list-one)
    (production :type-expression-list (:type-expression-list \, (:type-expression allow-in)) type-expression-list-more)
    
    
    (%subsection "Interface Definition")
    (production (:interface-definition :omega) (interface :qualified-identifier :extends-list :block) interface-definition-definition)
    (production (:interface-definition :omega) (interface :qualified-identifier (:semicolon :omega)) interface-definition-declaration)
    
    
    (%subsection "Namespace Definition")
    (production :namespace-definition (namespace :identifier :extends-list) namespace-definition-normal)
    
    (production :extends-list () extends-list-none)
    (production :extends-list (extends :type-expression-list) extends-list-one)
    
    
    ;(%subsection "Attribute Definition")
    ;(production :attribute-definition (attribute :no-line-break :identifier =) attribute-definition-none)
    ;(production :attribute-definition (default =) attribute-definition-default)
    ;(production :attribute-definition (:attribute-definition :attribute) attribute-definition-identifier)
    ;(production :attribute-definition (:attribute-definition namespace \( (:type-expression allow-in) \)) attribute-definition-namespace)
    
    
    (%section "Language Declaration")
    (production :language-declaration (use :language-alternatives) language-declaration-language-alternatives)
    
    (production :language-alternatives (:language-ids) language-alternatives-one)
    (production :language-alternatives (:language-alternatives \| :language-ids) language-alternatives-more)
    
    (production :language-ids () language-ids-none)
    (production :language-ids (:language-id :language-ids) language-ids-more)
    
    (production :language-id (:identifier) language-id-identifier)
    (production :language-id ($number) language-id-number)
    
    
    (%section "Package Definition")
    (production :package-definition (package :package-name :block) package-definition-named)
    
    
    (%section "Programs")
    (production :program (:top-statements) program-top-statements)))


(defparameter *jw* (generate-world "J" *jw-source* '((js2 . :js2) (es4 . :es4))))
(defparameter *jg* (world-grammar *jw* 'code-grammar))
(ensure-lf-subset *jg*)
(forward-parser-states *jg*)
#+allegro (clean-grammar *jg*) ;Remove this line if you wish to print the grammar's state tables.

(defparameter *ew* nil)
(defparameter *eg* nil)

(defun compute-ecma-subset ()
  (unless *ew*
    (setq *ew* (generate-world "E" *jw-source* '((js2 . delete) (es4 . nil))))
    (setq *eg* (world-grammar *ew* 'code-grammar))
    (ensure-lf-subset *eg*)
    (forward-parser-states *eg*))
  (length (grammar-states *eg*)))


; Print a list of states that have both $REGULAR-EXPRESSION and either / or /= as valid lookaheads.
(defun show-regexp-and-division-states (grammar)
  (all-state-transitions
   #'(lambda (state transitions-hash)
       (when (and (gethash '$regular-expression transitions-hash)
                  (or (gethash '/ transitions-hash) (gethash '/= transitions-hash)))
         (format *error-output* "State ~S~%" state)))
   grammar))


; Return five values:
;   A list of terminals that may precede a $regular-expression terminal;
;   A list of terminals that may precede a $virtual-semicolon but not / or /= terminal;
;   A list of terminals that may precede a / or /= terminal;
;   The intersection of the $regular-expression and /|/= lists.
;   The intersection of the $regular-expression|$virtual-semicolon and /|/= lists.
;
; USE ONLY ON canonical-lr-1 grammars.
; DON'T RUN THIS AFTER CALLING forward-parser-states.
(defun show-regexp-and-division-predecessors (grammar)
  (let* ((nstates (length (grammar-states grammar)))
         (state-predecessors (make-array nstates :element-type 'terminalset :initial-element *empty-terminalset*)))
    (dolist (state (grammar-states grammar))
      (dolist (transition-pair (state-transitions state))
        (let ((transition (cdr transition-pair)))
          (when (eq (transition-kind transition) :shift)
            (terminalset-union-f (svref state-predecessors (state-number (transition-state transition)))
                                 (make-terminalset grammar (car transition-pair)))))))
    (let ((regexp-predecessors *empty-terminalset*)
          (virtual-predecessors *empty-terminalset*)
          (div-predecessors *empty-terminalset*))
      (all-state-transitions
       #'(lambda (state transitions-hash)
           (let ((predecessors (svref state-predecessors (state-number state))))
             (when (gethash '$regular-expression transitions-hash)
               (terminalset-union-f regexp-predecessors predecessors))
             (if (or (gethash '/ transitions-hash) (gethash '/= transitions-hash))
               (terminalset-union-f div-predecessors predecessors)
               (when (gethash '$virtual-semicolon transitions-hash)
                 (terminalset-union-f virtual-predecessors predecessors)))))
       grammar)
      (values
       (terminalset-list grammar regexp-predecessors)
       (terminalset-list grammar virtual-predecessors)
       (terminalset-list grammar div-predecessors)
       (terminalset-list grammar (terminalset-intersection regexp-predecessors div-predecessors))
       (terminalset-list grammar (terminalset-intersection (terminalset-union regexp-predecessors virtual-predecessors) div-predecessors))))))


(defun depict-js-terminals (markup-stream grammar)
  (labels
    ((production-first-terminal (production)
       (first (production-rhs production)))
     
     (terminal-bin (terminal)
       (if (and terminal (symbolp terminal))
         (let ((name (symbol-name terminal)))
           (if (> (length name) 0)
             (let ((first-char (char name 0)))
               (cond
                ((char= first-char #\$) 0)
                ((not (or (char= first-char #\_) (alphanumericp first-char))) 1)
                ((member terminal (rule-productions (grammar-rule grammar ':identifier)) :key #'production-first-terminal) 5)
                (t 3)))
             1))
         1))
     
     (depict-terminal-bin (bin-name bin-terminals)
       (when bin-terminals
         (depict-paragraph (markup-stream ':body-text)
           (depict markup-stream bin-name)
           (depict-list markup-stream #'depict-terminal bin-terminals :separator '(" " :spc " "))))))
    
    (let* ((bins (make-array 6 :initial-element nil))
           (all-terminals (grammar-terminals grammar))
           (terminals (remove-if #'lf-terminal? all-terminals)))
      (assert-true (= (length all-terminals) (1- (* 2 (length terminals)))))
      (setf (svref bins 2) (list '\# '&&= '-> '@ '^^ '^^= '\|\|=))
      (setf (svref bins 4) (list 'abstract 'class 'const 'debugger 'enum 'export 'extends 'final 'goto 'implements 'import 'include
                                 'interface 'native 'package 'private 'protected 'public 'static 'super 'synchronized
                                 'throws 'transient 'volatile))
      (do ((i (length terminals)))
          ((zerop i))
        (let ((terminal (aref terminals (decf i))))
          (unless (eq terminal *end-marker*)
            (setf (svref bins 2) (delete terminal (svref bins 2)))
            (setf (svref bins 4) (delete terminal (svref bins 4)))
            (push terminal (svref bins (terminal-bin terminal))))))
      (depict-paragraph (markup-stream ':section-heading)
        (depict markup-stream "Terminals"))
      (mapc #'depict-terminal-bin '("General tokens: " "Punctuation tokens: " "Future punctuation tokens: "
                                    "Reserved words: " "Future reserved words: " "Non-reserved words: ")
            (coerce bins 'list)))))


#|
(values
 (length (grammar-states *jg*))
 (depict-rtf-to-local-file
  "JS20/ParserGrammarJS2.rtf"
  "JavaScript 2.0 Parser Grammar"
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *jg*)
      (depict-world-commands markup-stream *jw* :visible-semantics nil)))
 (compute-ecma-subset)
 (depict-rtf-to-local-file
  "JS20/ParserGrammarES4.rtf"
  "ECMAScript Edition 4 Parser Grammar"
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *eg*)
      (depict-world-commands markup-stream *ew* :visible-semantics nil))))

(values
 (length (grammar-states *jg*))
 (depict-html-to-local-file
  "JS20/ParserGrammarJS2.html"
  "JavaScript 2.0 Parser Grammar"
  t
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *jg*)
      (depict-world-commands markup-stream *jw* :visible-semantics nil)))
 (compute-ecma-subset)
 (depict-html-to-local-file
  "JS20/ParserGrammarES4.html"
  "ECMAScript Edition 4 Parser Grammar"
  t
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *eg*)
      (depict-world-commands markup-stream *ew* :visible-semantics nil))))

(with-local-output (s "JS20/ParserGrammarJS2 states") (print-grammar *jg* s))
(compute-ecma-subset)
(with-local-output (s "JS20/ParserGrammarES4 states") (print-grammar *eg* s))
|#

(length (grammar-states *jg*))
