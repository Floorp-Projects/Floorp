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
    (? js2
      (production :identifier (include) identifier-include))
    
    (production :qualifier (:identifier) qualifier-identifier)
    (production :qualifier (public) qualifier-public)
    (production :qualifier (private) qualifier-private)
    (production :qualifier (super) qualifier-super)
    (production :qualifier (:qualifier \:\: :identifier) qualifier-identifier-qualifier)
    
    (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier)
    (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier)
    
    (production :expression-qualified-identifier (:parenthesized-expression \:\: :identifier) expression-qualified-identifier-identifier)
    
    (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple)
    (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression)
    
    
    (%subsection "Primary Expressions")
    (production :primary-expression (null) primary-expression-null)
    (production :primary-expression (true) primary-expression-true)
    (production :primary-expression (false) primary-expression-false)
    (production :primary-expression (public) primary-expression-public)
    (production :primary-expression ($number) primary-expression-number)
    (production :primary-expression ($number :no-line-break $string) primary-expression-number-with-unit)
    (production :primary-expression ($string) primary-expression-string)
    (production :primary-expression (this) primary-expression-this)
    (production :primary-expression (super) primary-expression-super)
    (production :primary-expression ($regular-expression) primary-expression-regular-expression)
    (production :primary-expression (:parenthesized-list-expression) primary-expression-parenthesized-list-expression)
    (production :primary-expression (:parenthesized-list-expression :no-line-break $string) primary-expression-parenthesized-list-expression-with-unit)
    (production :primary-expression (:array-literal) primary-expression-array-literal)
    (production :primary-expression (:object-literal) primary-expression-object-literal)
    (production :primary-expression (:function-expression) primary-expression-function-expression)
    ;(production :primary-expression (eval :parenthesized-expression) primary-expression-eval)
    
    (production :parenthesized-expression (\( (:assignment-expression allow-in) \)) parenthesized-expression-assignment-expression)
    
    (production :parenthesized-list-expression (:parenthesized-expression) parenthesized-list-expression-parenthesized-expression)
    (production :parenthesized-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) parenthesized-list-expression-list-expression)
    
    
    (%subsection "Function Expressions")
    (production :function-expression (function :function-signature :block) function-expression-anonymous)
    (production :function-expression (function :identifier :function-signature :block) function-expression-named)
    
    
    (%subsection "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression)
    
    (production :field-name (:identifier) field-name-identifier)
    (production :field-name ($string) field-name-string)
    (production :field-name ($number) field-name-number)
    (production :field-name (:parenthesized-expression) field-name-parenthesized-expression)
    
    
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
    
    (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier)
    (production :attribute-expression (:attribute-expression :dot-operator) attribute-expression-dot-operator)
    (production :attribute-expression (:attribute-expression :brackets) attribute-expression-brackets)
    (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call1)
    
    (production :full-postfix-expression (:const-dot-expression) full-postfix-expression-const-dot-expression)
    (production :full-postfix-expression (:full-postfix-subexpression) full-postfix-expression-full-postfix-subexpression)
    
    (production :full-postfix-subexpression (:primary-expression) full-postfix-subexpression-primary-expression)
    (production :full-postfix-subexpression (:expression-qualified-identifier) full-postfix-subexpression-expression-qualified-identifier)
    (production :full-postfix-subexpression (:full-new-expression) full-postfix-subexpression-full-new-expression)
    (production :full-postfix-subexpression (:full-postfix-subexpression :dot-operator) full-postfix-subexpression-dot-operator)
    (production :full-postfix-subexpression (:full-postfix-expression :brackets) full-postfix-subexpression-brackets)
    (production :full-postfix-subexpression (:full-postfix-expression :arguments) full-postfix-subexpression-call)
    (production :full-postfix-subexpression (:postfix-expression :no-line-break ++) full-postfix-subexpression-increment)
    (production :full-postfix-subexpression (:postfix-expression :no-line-break --) full-postfix-subexpression-decrement)
    (? js2
      (production :full-postfix-subexpression (:postfix-expression @ :qualified-identifier) full-postfix-subexpression-coerce)
      (production :full-postfix-subexpression (:postfix-expression @ :parenthesized-expression) full-postfix-subexpression-indirect-coerce))
    
    (production :full-new-expression (new :bracket-expression :arguments) full-new-expression-new)
    
    (production :short-new-expression (new :short-new-subexpression) short-new-expression-new)
    (production :short-new-expression (const new :short-new-subexpression) short-new-expression-const-new)
    
    (production :short-new-subexpression (:bracket-expression) short-new-subexpression-new-full)
    (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short)
    
    
    (production :bracket-expression (:const-expression) bracket-expression-const-expression)
    (production :bracket-expression (:bracket-subexpression) bracket-expression-bracket-subexpression)
    
    (production :bracket-subexpression (:bracket-expression :brackets) bracket-subexpression-brackets)
    (production :bracket-subexpression (:bracket-subexpression :dot-operator) bracket-subexpression-dot-operator)
    
    (production :const-expression (:dot-expression) const-expression-dot-expression)
    (production :const-expression (:const-dot-expression) const-expression-const-dot-expression)
    
    (production :const-dot-expression (const :dot-expression) const-dot-expression-dot-expression)
    
    (production :dot-expression (:primary-expression) dot-expression-primary-expression)
    (production :dot-expression (:qualified-identifier) dot-expression-qualified-identifier)
    (production :dot-expression (:full-new-expression) dot-expression-full-new-expression)
    (production :dot-expression (:dot-expression :dot-operator) dot-expression-dot-operator)
    
    
    (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier)
    (production :dot-operator (\. class) dot-operator-class)
    
    (production :brackets ([ :argument-list ]) brackets-argument-list)
    
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
    (production :unary-expression (void :unary-expression) unary-expression-void)
    (production :unary-expression (typeof :unary-expression) unary-expression-typeof)
    (production :unary-expression (++ :postfix-expression) unary-expression-increment)
    (production :unary-expression (-- :postfix-expression) unary-expression-decrement)
    (production :unary-expression (+ :unary-expression) unary-expression-plus)
    (production :unary-expression (- :unary-expression) unary-expression-minus)
    (production :unary-expression (~ :unary-expression) unary-expression-bitwise-not)
    (production :unary-expression (! :unary-expression) unary-expression-logical-not)
    
    
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
    (production (:statement :omega) (:use-statement (:semicolon :omega)) statement-use-statement)
    (? js2
      (production (:statement :omega) (:include-statement (:semicolon :omega)) statement-include-statement))
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
    
    (production (:noninsertable-semicolon :omega_2) (\;) noninsertable-semicolon-semicolon)
    (production (:noninsertable-semicolon abbrev) () noninsertable-semicolon-abbrev)
    
    
    (%subsection "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%subsection "Expression Statement")
    (production :expression-statement ((:- function { const) (:list-expression allow-in)) expression-statement-list-expression)
    
    
    (%subsection "Block")
    (production :annotated-block (:attributes :block) annotated-block-attributes-and-block)
    
    (production :block ({ :top-statements }) block-top-statements)
    
    (production :top-statements () top-statements-none)
    (production :top-statements (:top-statements-prefix (:top-statement abbrev)) top-statements-more)
    
    (production :top-statements-prefix () top-statements-prefix-none)
    (production :top-statements-prefix (:top-statements-prefix (:top-statement full)) top-statements-prefix-more)
    
    
    (%subsection "Labeled Statements")
    (production (:labeled-statement :omega) (:identifier \: (:statement :omega)) labeled-statement-label)
    
    
    (%subsection "If Statement")
    (production (:if-statement abbrev) (if :parenthesized-list-expression (:statement abbrev)) if-statement-if-then-abbrev)
    (production (:if-statement full) (if :parenthesized-list-expression (:statement full)) if-statement-if-then-full)
    (production (:if-statement :omega) (if :parenthesized-list-expression (:statement abbrev-no-short-if)
                                           else (:statement :omega)) if-statement-if-then-else)
    
    
    (%subsection "Switch Statement")
    (production :switch-statement (switch :parenthesized-list-expression { :case-statements }) switch-statement-cases)
    
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
    (production :do-statement (do (:statement abbrev) while :parenthesized-list-expression) do-statement-do-while)
    
    
    (%subsection "While Statement")
    (production (:while-statement :omega) (while :parenthesized-list-expression (:statement :omega)) while-statement-while)
    
    
    (%subsection "For Statements")
    (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                             (:statement :omega)) for-statement-c-style)
    (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:statement :omega)) for-statement-in)
    
    (production :for-initializer () for-initializer-empty)
    (production :for-initializer ((:- const) (:list-expression no-in)) for-initializer-expression)
    (production :for-initializer (:attributes :variable-definition-kind (:variable-binding-list no-in)) for-initializer-variable-definition)
    
    (production :for-in-binding ((:- const) :postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:attributes :variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    
    
    (%subsection "With Statement")
    (production (:with-statement :omega) (with :parenthesized-list-expression (:statement :omega)) with-statement-with)
    
    
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
    
    (production :catch-clause (catch \( :parameter \) :annotated-block) catch-clause-block)
    
    (production :finally-clause (finally :annotated-block) finally-clause-block)
    
    
    (%subsection "Use Statement")
    (production :use-statement (use :no-line-break namespace :nonassignment-expression-list) use-statement-normal)
    
    (production :nonassignment-expression-list ((:non-assignment-expression allow-in)) nonassignment-expression-list-one)
    (production :nonassignment-expression-list (:nonassignment-expression-list \, (:non-assignment-expression allow-in)) nonassignment-expression-list-more)
    
    
    (? js2
      (%subsection "Include Statement")
      (production :include-statement (include :no-line-break $string) include-statement-include))
    
    
    (%section "Definitions")
    (production (:annotated-definition :omega) (:attributes (:definition :omega)) annotated-definition-attribute-and-definition)
    
    (production :attributes () attributes-none)
    (production :attributes (:attribute :no-line-break :attributes) attributes-more)
    
    (production :attribute (:attribute-expression) attribute-attribute-expression)
    (production :attribute (abstract) attribute-abstract)
    (production :attribute (final) attribute-final)
    (production :attribute (private) attribute-private)
    (production :attribute (public) attribute-public)
    (production :attribute (static) attribute-static)
    
    (production (:definition :omega) (:import-definition (:semicolon :omega)) definition-import-definition)
    (production (:definition :omega) (:export-definition (:semicolon :omega)) definition-export-definition)
    (production (:definition :omega) (:variable-definition (:semicolon :omega)) definition-variable-definition)
    (production (:definition :omega) ((:function-definition :omega)) definition-function-definition)
    (production (:definition :omega) ((:class-definition :omega)) definition-class-definition)
    (production (:definition :omega) (:namespace-definition (:semicolon :omega)) definition-namespace-definition)
    (? js2
      (production (:definition :omega) ((:interface-definition :omega)) definition-interface-definition))
    
    
    (%subsection "Import Definition")
    (production :import-definition (import :import-binding) import-definition-import)
    (production :import-definition (import :import-binding \: :nonassignment-expression-list) import-definition-import-and-use)
    
    (production :import-binding (:import-item) import-binding-import-item)
    (production :import-binding (:identifier = :import-item) import-binding-named-import-item)
    
    (production :import-item ($string) import-item-string)
    (production :import-item (:package-name) import-item-package-name)
    
    
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
    (production (:variable-binding :beta) ((:typed-variable :beta) = :multiple-attributes) variable-binding-multiple-attributes)
    
    (production :multiple-attributes (:attribute :no-line-break :attribute) multiple-attributes-two)
    (production :multiple-attributes (:multiple-attributes :no-line-break :attribute) multiple-attributes-more)
    
    (production (:typed-variable :beta) (:identifier) typed-variable-identifier)
    (production (:typed-variable :beta) (:identifier \: (:type-expression :beta)) typed-variable-identifier-and-type)
    ;(production (:typed-variable :beta) ((:type-expression :beta) :identifier) typed-variable-type-and-identifier)
    
    
    (%subsection "Function Definition")
    (production (:function-definition :omega) (:function-declaration :block) function-definition-definition)
    (production (:function-definition :omega) (:function-declaration (:semicolon :omega)) function-definition-declaration)
    
    (production :function-declaration (function :function-name :function-signature) function-declaration-signature-and-body)
    
    (production :function-name (:identifier) function-name-function)
    (production :function-name (get :no-line-break :identifier) function-name-getter)
    (production :function-name (set :no-line-break :identifier) function-name-setter)
    
    (production :function-signature (:parameter-signature :result-signature) function-signature-parameter-and-result-signatures)
    
    (production :parameter-signature (\( :parameters \)) parameter-signature-parameters)
    
    (production :parameters () parameters-none)
    (production :parameters (:all-parameters) parameters-all-parameters)
    
    (production :all-parameters (:parameter) all-parameters-parameter)
    (production :all-parameters (:parameter \, :all-parameters) all-parameters-parameter-and-more)
    (production :all-parameters (:optional-named-rest-parameters) all-parameters-optional-named-rest-parameters)
    
    (production :optional-named-rest-parameters (:optional-parameter) optional-named-rest-parameters-optional-parameter)
    (production :optional-named-rest-parameters (:optional-parameter \, :optional-named-rest-parameters) optional-named-rest-parameters-optional-parameter-and-more)
    (production :optional-named-rest-parameters (\| :named-rest-parameters) optional-named-rest-parameters-named-rest-parameters)
    (production :optional-named-rest-parameters (:rest-parameter) optional-named-rest-parameters-rest-parameter)
    (production :optional-named-rest-parameters (:rest-parameter \, \| :named-parameters) optional-named-rest-parameters-rest-and-named)
    
    (production :named-rest-parameters (:named-parameter) named-rest-parameters-named-parameter)
    (production :named-rest-parameters (:named-parameter \, :named-rest-parameters) named-rest-parameters-named-parameter-and-more)
    (production :named-rest-parameters (:rest-parameter) named-rest-parameters-rest-parameter)
    
    (production :named-parameters (:named-parameter) named-parameters-named-parameter)
    (production :named-parameters (:named-parameter \, :named-parameters) named-parameters-named-parameter-and-more)
    
    (production :rest-parameter (\.\.\.) rest-parameter-none)
    (production :rest-parameter (\.\.\. :parameter) rest-parameter-parameter)
    ;(production :rest-parameter (\.\.\. :optional-parameter) rest-parameter-optional-parameter)
    
    (production :parameter (:identifier) parameter-identifier)
    (production :parameter (:identifier \: (:type-expression allow-in)) parameter-identifier-and-type)
    ;(production :parameter ((:- $string) (:type-expression allow-in) :identifier) parameter-type-and-identifier)
    
    (production :optional-parameter (:parameter = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
    
    (production :named-parameter (:parameter) named-parameter-parameter)
    (production :named-parameter (:optional-parameter) named-parameter-optional-parameter)
    (production :named-parameter ($string :named-parameter) named-parameter-name)
    
    (production :result-signature () result-signature-none)
    (production :result-signature (\: (:type-expression allow-in)) result-signature-colon-and-type-expression)
    ;(production :result-signature ((:- {) (:type-expression allow-in)) result-signature-type-expression)
    
    
    (%subsection "Class Definition")
    (production (:class-definition :omega) (class :identifier :inheritance :block) class-definition-definition)
    (production (:class-definition :omega) (class :identifier (:semicolon :omega)) class-definition-declaration)
    
    (production :inheritance () inheritance-none)
    (production :inheritance (extends (:type-expression allow-in)) inheritance-extends)
    (? js2
      (production :inheritance (implements :type-expression-list) inheritance-implements)
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements)
      
      (%subsection "Interface Definition")
      (production (:interface-definition :omega) (interface :identifier :extends-list :block) interface-definition-definition)
      (production (:interface-definition :omega) (interface :identifier (:semicolon :omega)) interface-definition-declaration))
    
    
    (%subsection "Namespace Definition")
    (production :namespace-definition (namespace :identifier :extends-list) namespace-definition-normal)
    
    (production :extends-list () extends-list-none)
    (production :extends-list (extends :type-expression-list) extends-list-one)
    
    (production :type-expression-list ((:type-expression allow-in)) type-expression-list-one)
    (production :type-expression-list (:type-expression-list \, (:type-expression allow-in)) type-expression-list-more)
    
    
    (%section "Language Declaration")
    (production :language-declaration (use :language-alternatives) language-declaration-language-alternatives)
    
    (production :language-alternatives (:language-ids) language-alternatives-one)
    (production :language-alternatives (:language-alternatives \| :language-ids) language-alternatives-more)
    
    (production :language-ids () language-ids-none)
    (production :language-ids (:language-id :language-ids) language-ids-more)
    
    (production :language-id (:identifier) language-id-identifier)
    (production :language-id ($number) language-id-number)
    
    
    (%section "Package Definition")
    (production :package-definition (package :block) package-definition-anonymous)
    (production :package-definition (package :package-name :block) package-definition-named)
    
    (production :package-name (:identifier) package-name-one)
    (production :package-name (:package-name \. :identifier) package-name-more)
    
    
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
      (setf (svref bins 4) (list 'abstract 'class 'const 'debugger 'enum 'export 'extends 'final 'goto 'implements 'import
                                 'interface 'native 'package 'private 'protected 'public 'static 'super 'synchronized
                                 'throws 'transient 'volatile))
      ; Used to be reserved in JavaScript 1.5: 'boolean 'byte 'char 'double 'float 'int 'long 'short
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
