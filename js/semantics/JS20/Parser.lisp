;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(progn
  (defparameter *jw*
    (generate-world
     "J"
     '((line-grammar code-grammar :lalr-1 :program)
       
       
       (%section "Expressions")
       (grammar-argument :beta allow-in no-in)
       
       (%subsection "Identifiers")
       (production :identifier ($identifier) identifier-identifier)
       (production :identifier (get) identifier-get)
       (production :identifier (set) identifier-set)
       (production :identifier (language) identifier-language)

       (production :qualified-identifier (:identifier) qualified-identifier-identifier)
       (production :qualified-identifier (:qualifier \:\: :qualified-identifier) qualified-identifier-qualifier)

       (production :qualifier (:identifier) qualifier-identifier)
       (production :qualifier (:parenthesized-expression) qualifier-parenthesized-expression)
       
       
       (%subsection "Primary Expressions")
       (production :primary-expression (null) primary-expression-null)
       (production :primary-expression (true) primary-expression-true)
       (production :primary-expression (false) primary-expression-false)
       (production :primary-expression ($number) primary-expression-number)
       (production :primary-expression ($number :no-line-break $string) primary-expression-number-with-unit)
       (production :primary-expression ($string) primary-expression-string)
       (production :primary-expression (this) primary-expression-this)
       (production :primary-expression (super) primary-expression-super)
       (production :primary-expression (:qualified-identifier) primary-expression-qualified-identifier)
       (production :primary-expression ($regular-expression) primary-expression-regular-expression)
       (production :primary-expression (:parenthesized-expression) primary-expression-parenthesized-expression)
       (production :primary-expression (:parenthesized-expression :no-line-break $string) primary-expression-parenthesized-expression-with-unit)
       (production :primary-expression (:array-literal) primary-expression-array-literal)
       (production :primary-expression (:object-literal) primary-expression-object-literal)
       (production :primary-expression (:function-expression) primary-expression-function-expression)
       
       (production :parenthesized-expression (\( (:expression allow-in) \)) parenthesized-expression-expression)
       
       
       (%subsection "Function Expressions")
       (production :function-expression (function :function-signature :block) function-expression-anonymous)
       (production :function-expression (function :identifier :function-signature :block) function-expression-named)
       
       
       (%subsection "Object Literals")
       (production :object-literal (\{ \}) object-literal-empty)
       (production :object-literal (\{ :field-list \}) object-literal-list)
       
       (production :field-list (:literal-field) field-list-one)
       (production :field-list (:field-list \, :literal-field) field-list-more)
       
       (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression)
       
       (production :field-name (:qualified-identifier) field-name-qualified-identifier)
       (production :field-name ($string) field-name-string)
       (production :field-name ($number) field-name-number)
       
       
       (%subsection "Array Literals")
       (production :array-literal ([ :element-list ]) array-literal-list)
       
       (production :element-list (:literal-element) element-list-one)
       (production :element-list (:element-list \, :literal-element) element-list-more)
       
       (production :literal-element () literal-element-none)
       (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
       
       
       (%subsection "Postfix Unary Operators")
       (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression)
       (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression)
       
       (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression)
       (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression)
       (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator)
       (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call)
       (production :full-postfix-expression (:postfix-expression :no-line-break ++) full-postfix-expression-increment)
       (production :full-postfix-expression (:postfix-expression :no-line-break --) full-postfix-expression-decrement)
       
       (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new)
       
       (production :short-new-expression (new :short-new-subexpression) short-new-expression-new)
       
       (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression)
       (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator)
       (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression)
       
       (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full)
       (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short)
       
       (production :member-operator ([ :argument-list ]) member-operator-array)
       (production :member-operator (\. :qualified-identifier) member-operator-property)
       (production :member-operator (\. :parenthesized-expression) member-operator-indirect-property)
       (production :member-operator (@ :qualified-identifier) member-operator-coerce)
       (production :member-operator (@ :parenthesized-expression) member-operator-indirect-coerce)
       
       (production :arguments (\( :argument-list \)) arguments-argument-list)
       
       (production :argument-list () argument-list-none)
       (production :argument-list (:argument-list-prefix) argument-list-unnamed)
       (production :argument-list (:named-argument-list-prefix) argument-list-named)
       
       (production :argument-list-prefix ((:assignment-expression allow-in)) argument-list-prefix-one)
       (production :argument-list-prefix (:argument-list-prefix \, (:assignment-expression allow-in)) argument-list-prefix-more)
       
       (production :named-argument-list-prefix (:literal-field) named-argument-list-prefix-one)
       (production :named-argument-list-prefix (:argument-list-prefix \, :literal-field) named-argument-list-prefix-unnamed)
       (production :named-argument-list-prefix (:named-argument-list-prefix \, :literal-field) named-argument-list-prefix-more)
       
       
       (%subsection "Prefix Unary Operators")
       (production :unary-expression (:postfix-expression) unary-expression-postfix)
       (production :unary-expression (delete :postfix-expression) unary-expression-delete)
       (production :unary-expression (typeof :unary-expression) unary-expression-typeof)
       (production :unary-expression (eval :unary-expression) unary-expression-eval)
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
       ;(production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ *) bitwise-xor-expression-null)
       ;(production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ ?) bitwise-xor-expression-undefined)
       
       (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor)
       (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| (:bitwise-xor-expression :beta)) bitwise-or-expression-or)
       ;(production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| *) bitwise-or-expression-null)
       ;(production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| ?) bitwise-or-expression-undefined)
       
       
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
       
       
       (%subsection "Expressions")
       (production (:expression :beta) ((:assignment-expression :beta)) expression-assignment)
       (production (:expression :beta) ((:expression :beta) \, (:assignment-expression :beta)) expression-comma)
       
       (production :optional-expression ((:expression allow-in)) optional-expression-expression)
       (production :optional-expression () optional-expression-empty)
       
       
       (%subsection "Type Expressions")
       (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression)
       
       
       (%section "Statements")
       
       (grammar-argument :omega
                         abbrev              ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                         abbrev-non-empty    ;optional semicolon as long as statement isn't empty
                         abbrev-no-short-if  ;optional semicolon, but statement must not end with an if without an else
                         full)               ;semicolon required at the end
       (grammar-argument :omega_3 abbrev abbrev-non-empty full)
       (grammar-argument :omega_2 abbrev full)
       
       (production (:top-statement :omega_3) ((:statement :omega_3)) top-statement-statement)
       (production (:top-statement :omega_3) ((:language-declaration :omega_3)) top-statement-language-declaration)
       (production (:top-statement :omega_3) (:package-definition) top-statement-package-definition)
       
       (production (:statement :omega) ((:annotated-definition :omega)) statement-annotated-definition)
       (production (:statement :omega) ((:empty-statement :omega)) statement-empty-statement)
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
       #|(production (:statement :omega) ((:import-statement :omega)) statement-import-statement)|#
       
       (production (:semicolon :omega) (\;) semicolon-semicolon)
       (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
       (production (:semicolon abbrev) () semicolon-abbrev)
       (production (:semicolon abbrev-non-empty) () semicolon-abbrev-non-empty)
       (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
       
       
       (%subsection "Empty Statement")
       (production (:empty-statement :omega) (\;) empty-statement-semicolon)
       (production (:empty-statement abbrev) () empty-statement-abbrev)
       
       
       (%subsection "Expression Statement")
       (production :expression-statement ((:- function {) (:expression allow-in)) expression-statement-expression)
       
       
       (%subsection "Block")
       (production :annotated-block (:attributes :block) annotated-block-attributes-block)
       
       (production :block ({ :top-statements }) block-top-statements)
       
       (production :top-statements ((:top-statement abbrev)) top-statements-one)
       (production :top-statements (:top-statements-prefix (:top-statement abbrev-non-empty)) top-statements-more)
       
       (production :top-statements-prefix ((:top-statement full)) top-statements-prefix-one)
       (production :top-statements-prefix (:top-statements-prefix (:top-statement full)) top-statements-prefix-more)
       
       
       (%subsection "Labeled Statements")
       (production (:labeled-statement :omega) (:identifier \: (:statement :omega)) labeled-statement-label)
       
       
       (%subsection "If Statement")
       (production (:if-statement abbrev) (if :parenthesized-expression (:statement abbrev)) if-statement-if-then-abbrev)
       (production (:if-statement abbrev-non-empty) (if :parenthesized-expression (:statement abbrev-non-empty)) if-statement-if-then-abbrev-non-empty)
       (production (:if-statement full) (if :parenthesized-expression (:statement full)) if-statement-if-then-full)
       (production (:if-statement :omega) (if :parenthesized-expression (:statement abbrev-no-short-if)
                                              else (:statement :omega)) if-statement-if-then-else)
       
       
       (%subsection "Switch Statement")
       (production :switch-statement (switch :parenthesized-expression { }) switch-statement-empty)
       (production :switch-statement (switch :parenthesized-expression { :case-groups :last-case-group }) switch-statement-cases)
       
       (production :case-groups () case-groups-empty)
       (production :case-groups (:case-groups :case-group) case-groups-more)
       
       (production :case-group (:case-guards :case-statements-prefix) case-group-case-statements-prefix)
       
       (production :last-case-group (:case-guards :case-statements) last-case-group-case-statements)
       
       (production :case-guards (:case-guard) case-guards-one)
       (production :case-guards (:case-guards :case-guard) case-guards-more)
       
       (production :case-guard (case (:expression allow-in) \:) case-guard-case)
       (production :case-guard (default \:) case-guard-default)
       
       (production :case-statements ((:statement abbrev)) case-statements-one)
       (production :case-statements (:case-statements-prefix (:statement abbrev-non-empty)) case-statements-more)
       
       (production :case-statements-prefix ((:statement full)) case-statements-prefix-one)
       (production :case-statements-prefix (:case-statements-prefix (:statement full)) case-statements-prefix-more)
       
       
       (%subsection "Do-While Statement")
       (production :do-statement (do (:statement abbrev-non-empty) while :parenthesized-expression) do-statement-do-while)
       
       
       (%subsection "While Statement")
       (production (:while-statement :omega) (while :parenthesized-expression (:statement :omega)) while-statement-while)
       
       
       (%subsection "For Statements")
       (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                                (:statement :omega)) for-statement-c-style)
       (production (:for-statement :omega) (for \( :for-in-binding in (:expression allow-in) \) (:statement :omega)) for-statement-in)
       
       (production :for-initializer () for-initializer-empty)
       (production :for-initializer ((:expression no-in)) for-initializer-expression)
       (production :for-initializer (:variable-definition-kind (:variable-binding-list no-in)) for-initializer-variable-definition)
       
       (production :for-in-binding (:postfix-expression) for-in-binding-expression)
       (production :for-in-binding (:variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
       
       
       (%subsection "With Statement")
       (production (:with-statement :omega) (with :parenthesized-expression (:statement :omega)) with-statement-with)
       
       
       (%subsection "Continue and Break Statements")
       (production :continue-statement (continue :no-line-break :optional-label) continue-statement-optional-label)
       
       (production :break-statement (break :no-line-break :optional-label) break-statement-optional-label)
       
       (production :optional-label () optional-label-default)
       (production :optional-label (:identifier) optional-label-identifier)
       
       
       (%subsection "Return Statement")
       (production :return-statement (return :no-line-break :optional-expression) return-statement-optional-expression)
       
       
       (%subsection "Throw Statement")
       (production :throw-statement (throw :no-line-break (:expression allow-in)) throw-statement-throw)
       
       
       (%subsection "Try Statement")
       (production :try-statement (try :annotated-block :catch-clauses) try-statement-catch-clauses)
       (production :try-statement (try :annotated-block :finally-clause) try-statement-finally-clause)
       (production :try-statement (try :annotated-block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
       
       (production :catch-clauses (:catch-clause) catch-clauses-one)
       (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
       
       (production :catch-clause (catch \( (:typed-identifier allow-in) \) :annotated-block) catch-clause-block)
       
       (production :finally-clause (finally :annotated-block) finally-clause-block)
       
       #|
       (%subsection "Import Statement")
       (production (:import-statement :omega) (import :import-list (:semicolon :omega)) import-statement-import)
       (production (:import-statement abbrev) (import :import-list :block) import-statement-import-then-abbrev)
       (production (:import-statement abbrev-non-empty) (import :import-list :block) import-statement-import-then-abbrev-non-empty)
       (production (:import-statement full) (import :import-list :block) import-statement-import-then-full)
       (production (:import-statement :omega) (import :import-list :block else (:statement :omega)) import-statement-import-then-else)
       
       (production :import-list (:import-item) import-list-one)
       (production :import-list (:import-list \, :import-item) import-list-more)
       
       (production :import-item (:import-source) import-item-import-source)
       (production :import-item (:identifier = :import-source) import-item-named-import-source)
       (production :import-item (protected :identifier = :import-source) import-item-protected-import-source)
       
       (production :import-source ((:non-assignment-expression no-in)) import-source-no-version)
       (production :import-source ((:non-assignment-expression no-in) \: $string) import-source-version)
       |#
       
       (%section "Definitions")
       (production (:annotated-definition :omega) (:attributes (:definition :omega)) annotated-definition-attributes-and-definition)

       (production :attributes () attributes-none)
       (production :attributes (:fixed-attribute :no-line-break :attributes) attributes-fixed-attribute-more)
       (production :attributes ($identifier :no-line-break :attributes) attributes-identifier-more)
       (production :attributes (get :no-line-break :attributes) attributes-get-more)
       (production :attributes (set :no-line-break :attributes) attributes-set-more)
       (production :attributes (language :no-line-break :attributes) attributes-language-more)
       
       (%text :grammar
         "The third through sixth " (:grammar-symbol :attributes) " productions are merely the result of manually inlining the "
         (:grammar-symbol :identifier) " rule inside "
         (:grammar-symbol :attributes) " " :derives-10 " " (:grammar-symbol :identifier) " " :no-line-break " " (:grammar-symbol :attributes)
         ". Without manually inlining the " (:grammar-symbol :identifier) " rule here the grammar would not be LR(1).")

       (production :fixed-attribute (private) fixed-attribute-private)
       (production :fixed-attribute (public) fixed-attribute-public)
       (production :fixed-attribute (final) fixed-attribute-final)
       ;(production :fixed-attribute (static) fixed-attribute-static)
       
       (production (:definition :omega) (:variable-definition (:semicolon :omega)) definition-variable-definition)
       (production (:definition :omega) ((:function-definition :omega)) definition-function-definition)
       (production (:definition :omega) (:class-definition) definition-class-definition)

       #|
       (%subsection "Visibility Specifications")
       (production :visibility (:identifier) visibility-qualified-identifier)
       (production :visibility (local) visibility-local)
       (production :visibility (box) visibility-box)
       (production :visibility (private) visibility-private)
       (production :visibility (package) visibility-package)
       (production :visibility (public) visibility-public)
       (production :visibility ($identifier) visibility-user-defined)
       (production :visibility (public :versions-and-renames) visibility-public)
       
       (production :versions-and-renames () versions-and-renames-none)
       (production :versions-and-renames (:versions-and-renames-prefix) versions-and-renames-some)
       
       (production :versions-and-renames-prefix (:version-range-and-alias) versions-and-renames-prefix-one)
       (production :versions-and-renames-prefix (:versions-and-renames-prefix \, :version-range-and-alias) versions-and-renames-prefix-more)
       
       (production :version-range-and-alias (:version-range) version-range-and-alias-version-range)
       (production :version-range-and-alias (:version-range \: :identifier) version-range-and-alias-version-range-and-alias)
       
       (production :version-range (:version) version-range-singleton)
       (production :version-range (:optional-version \.\. :optional-version) version-range-range)
       
       
       (%subsection "Versions")
       (production :optional-version (:version) optional-version-version)
       (production :optional-version () optional-version-none)
       
       (production :version ($string) version-string)
       
       
       (%subsection "Version Definition")
       (production :version-definition (version :version) version-definition-version)
       (production :version-definition (version :version > :version-list) version-definition-ranked-version)
       (production :version-definition (version :version = :version) version-definition-alias)
       
       (production :version-list (:version) version-list-one)
       (production :version-list (:version-list \, :version) version-list-more)
       |#
       
       
       (%subsection "Variable Definition")
       (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition)
       
       (production :variable-definition-kind (var) variable-definition-kind-var)
       (production :variable-definition-kind (const) variable-definition-kind-const)
       
       (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one)
       (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more)
       
       (production (:variable-binding :beta) ((:typed-identifier :beta) (:variable-initializer :beta)) variable-binding-initializer)
       
       (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier)
       (production (:typed-identifier :beta) (:identifier \: (:type-expression :beta)) typed-identifier-identifier-and-type)
       ;(production (:typed-identifier :beta) ((:type-expression :beta) :identifier) typed-identifier-type-and-identifier)
       
       (production (:variable-initializer :beta) () variable-initializer-empty)
       (production (:variable-initializer :beta) (= (:assignment-expression :beta)) variable-initializer-assignment-expression)
       
       
       (%subsection "Function Definition")
       (production (:function-definition :omega) (:concrete-function-definition) function-definition-concrete)
       (production (:function-definition :omega) ((:abstract-function-definition :omega)) function-definition-abstract)
       
       (production :concrete-function-definition (function :function-name :function-signature :block) concrete-function-definition-signature-and-body)
       
       (production (:abstract-function-definition :omega) (function :function-name :function-signature (:semicolon :omega)) abstract-function-definition-signature)
       
       (production :function-name (:identifier) function-name-function)
       (production :function-name (get :no-line-break :identifier) function-name-getter)
       (production :function-name (set :no-line-break :identifier) function-name-setter)
       (production :function-name (new :no-line-break :identifier) function-name-constructor)
       (production :function-name (new) function-name-default-constructor)

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
       
       (production :required-parameter ((:typed-identifier allow-in)) required-parameter-typed-identifier)
       
       (production :optional-parameter ((:typed-identifier allow-in) = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
       
       (production :rest-parameter (\.\.\.) rest-parameter-none)
       (production :rest-parameter (\.\.\. (:typed-identifier allow-in)) rest-parameter-typed-identifier)
       (production :rest-parameter (\.\.\. (:typed-identifier allow-in) = (:assignment-expression allow-in)) rest-parameter-assignment-expression)
       
       (production :result-signature () result-signature-none)
       (production :result-signature (\: (:type-expression allow-in)) result-signature-colon-and-type-expression)
       ;(production :result-signature ((:- {) (:type-expression allow-in)) result-signature-type-expression)
       
       
       (%subsection "Class Definition")
       (production :class-definition (:class-definition-kind :identifier :superclasses :implementees :block) class-definition-normal)
       
       (production :class-definition-kind (class) class-definition-kind-class)
       (production :class-definition-kind (interface) class-definition-kind-interface)

       (production :superclasses () superclasses-none)
       (production :superclasses (extends :superclass-list) superclasses-one)
       
       (production :superclass-list ((:type-expression allow-in)) superclass-list-one)
       (production :superclass-list (:superclass-list \, (:type-expression allow-in)) superclass-list-more)

       (production :implementees () implementees-none)
       (production :implementees (implements :implementee-list) implementees-one)
       
       (production :implementee-list ((:type-expression allow-in)) implementee-list-one)
       (production :implementee-list (:implementee-list \, (:type-expression allow-in)) implementee-list-more)
       
       
       (%section "Language Declaration")
       (production (:language-declaration :omega_3) (language :no-line-break :language-ids :language-alternatives (:language-semicolon :omega_3))
                   language-declaration-one-or-more)
       
       (production (:language-semicolon :omega_3) (\;) language-semicolon-semicolon)
       (production (:language-semicolon abbrev) () language-semicolon-abbrev)
       (production (:language-semicolon abbrev-non-empty) () language-semicolon-abbrev-non-empty)
       
       (production :language-alternatives () language-alternatives-none)
       (production :language-alternatives (\|) language-alternatives-empty)
       (production :language-alternatives (\| :language-ids :language-alternatives) language-alternatives-more)
       
       (production :language-ids ($identifier :language-ids-rest) language-ids-identifier-more)
       (production :language-ids (get :language-ids-rest) language-ids-get-more)
       (production :language-ids (set :language-ids-rest) language-ids-set-more)
       (production :language-ids (language :language-ids-rest) language-ids-language-more)
       (production :language-ids ($number :language-ids-rest) language-ids-number-more)
       
       (production :language-ids-rest () language-ids-rest-none)
       (production :language-ids-rest (:no-line-break :language-ids) language-ids-rest-some)
       
       (%text :grammar
         "The first through fourth " (:grammar-symbol :language-ids) " productions are merely the result of manually inlining the "
         (:grammar-symbol :identifier) " rule inside "
         (:grammar-symbol :language-ids) " " :derives-10 " " (:grammar-symbol :identifier) " " (:grammar-symbol :language-ids-rest)
         ". Without manually inlining the " (:grammar-symbol :identifier) " rule here the grammar would not be LR(1).")
       
       
       (%section "Package Definition")
       (production :package-definition (package :identifier :block) package-definition-named)
       
       (%section "Programs")
       
       (production :program (:top-statements) program-top-statements)
       )))
  
  (defparameter *jg* (world-grammar *jw* 'code-grammar)))



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
    ((terminal-bin (terminal)
       (if (and terminal (symbolp terminal))
         (let ((name (symbol-name terminal)))
           (if (> (length name) 0)
             (let ((first-char (char name 0)))
               (cond
                ((char= first-char #\$) 0)
                ((not (or (char= first-char #\_) (alphanumericp first-char))) 1)
                ((member terminal (rule-productions (grammar-rule grammar ':identifier))
                         :key #'(lambda (production) (first (production-rhs production))))
                 5)
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
                                 'interface 'native 'package 'private 'protected 'public #|'static|# 'super 'synchronized
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
(depict-rtf-to-local-file
 "JS20/ParserGrammar.rtf"
 "JavaScript 2.0 Parser Grammar"
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg*)
     (depict-world-commands markup-stream *jw* :visible-semantics nil)))

(depict-html-to-local-file
 "JS20/ParserGrammar.html"
 "JavaScript 2.0 Parser Grammar"
 t
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg*)
     (depict-world-commands markup-stream *jw* :visible-semantics nil)))

(with-local-output (s "JS20/ParserGrammar.txt") (print-grammar *jg* s))
|#

(ensure-lf-subset *jg*)
(forward-parser-states *jg*)
#+allegro (clean-grammar *jg*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *jg*))
