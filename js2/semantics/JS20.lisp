;;; The contents of this file are subject to the Netscape Public License
;;; Version 1.0 (the "NPL"); you may not use this file except in
;;; compliance with the NPL.  You may obtain a copy of the NPL at
;;; http://www.mozilla.org/NPL/
;;;
;;; Software distributed under the NPL is distributed on an "AS IS" basis,
;;; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
;;; for the specific language governing rights and limitations under the
;;; NPL.
;;;
;;; The Initial Developer of this code under the NPL is Netscape
;;; Communications Corporation.  Portions created by Netscape are
;;; Copyright (C) 1999 Netscape Communications Corporation.  All Rights
;;; Reserved.

;;;
;;; Sample JavaScript 2.0 grammar
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;

(declaim (optimize (debug 3)))

(progn
  (defparameter *jw*
    (generate-world
     "J"
     '((grammar code-grammar :lr-1 :program)
       
       
       (%section "Expressions")
       (grammar-argument :alpha normal initial)
       (grammar-argument :beta allow-in no-in)
       
       (%subsection "Identifiers")
       (production :identifier ($identifier) identifier-identifier)
       ;(production :identifier (const) identifier-const)
       (production :identifier (version) identifier-version)
       (production :identifier (override) identifier-override)
       ;(production :identifier (field) identifier-field)
       (production :identifier (method) identifier-method)
       (production :identifier (constructor) identifier-constructor)
       
       (production :qualified-identifier (:identifier) qualified-identifier-identifier)
       (production :qualified-identifier (:qualified-identifier \:\: :identifier) qualified-identifier-multi-level)
       (production :qualified-identifier (:parenthesized-expression \:\: :identifier) qualified-identifier-parenthesized-expression)
       
       
       (%subsection "Primary Expressions")
       (production (:primary-expression :alpha) (:simple-expression) primary-expression-simple-expression)
       (production (:primary-expression normal) (:function-expression) primary-expression-function-expression)
       (production (:primary-expression normal) (:object-literal) primary-expression-object-literal)
       
       (production :simple-expression (this) simple-expression-this)
       (production :simple-expression (null) simple-expression-null)
       (production :simple-expression (true) simple-expression-true)
       (production :simple-expression (false) simple-expression-false)
       (production :simple-expression (*) simple-expression-any-type)
       (production :simple-expression ($number) simple-expression-number)
       (production :simple-expression ($string) simple-expression-string)
       (production :simple-expression (:qualified-identifier) simple-expression-qualified-identifier)
       (production :simple-expression ($regular-expression) simple-expression-regular-expression)
       (production :simple-expression (:parenthesized-expression) simple-expression-parenthesized-expression)
       (production :simple-expression (:array-literal) simple-expression-array-literal)
       
       (production :parenthesized-expression (\( (:expression normal allow-in) \)) parenthesized-expression-expression)
       
       
       (%subsection "Function Expressions")
       (production :function-expression (:anonymous-function) function-expression-anonymous-function)
       (production :function-expression (:named-function) function-expression-named-function)
       
       
       (%subsection "Object Literals")
       (production :object-literal (\{ \}) object-literal-empty)
       (production :object-literal (\{ :field-list \}) object-literal-list)
       
       (production :field-list (:literal-field) field-list-one)
       (production :field-list (:field-list \, :literal-field) field-list-more)
       
       (production :literal-field (:qualified-identifier \: (:assignment-expression normal allow-in)) literal-field-assignment-expression)
       
       
       (%subsection "Array Literals")
       (production :array-literal ([ ]) array-literal-empty)
       (production :array-literal ([ :element-list ]) array-literal-list)
       
       (production :element-list (:literal-element) element-list-one)
       (production :element-list (:element-list \, :literal-element) element-list-more)
       
       (production :literal-element ((:assignment-expression normal allow-in)) literal-element-assignment-expression)
       
       
       (%subsection "Postfix Unary Operators")
       (production (:postfix-expression :alpha) ((:full-postfix-expression :alpha)) postfix-expression-full-postfix-expression)
       (production (:postfix-expression :alpha) (:short-new-expression) postfix-expression-short-new-expression)
       
       (production (:full-postfix-expression :alpha) ((:primary-expression :alpha)) full-postfix-expression-primary-expression)
       (production (:full-postfix-expression :alpha) (:full-new-expression) full-postfix-expression-full-new-expression)
       (production (:full-postfix-expression :alpha) ((:full-postfix-expression :alpha) :member-operator) full-postfix-expression-member-operator)
       (production (:full-postfix-expression :alpha) ((:full-postfix-expression :alpha) :arguments) full-postfix-expression-call)
       (production (:full-postfix-expression :alpha) ((:postfix-expression :alpha) ++) full-postfix-expression-increment)
       (production (:full-postfix-expression :alpha) ((:postfix-expression :alpha) --) full-postfix-expression-decrement)
       (production (:full-postfix-expression :alpha) ((:postfix-expression :alpha) !) full-postfix-expression-null-augmented-type)
       (production (:full-postfix-expression :alpha) ((:postfix-expression :alpha) ~) full-postfix-expression-undefined-augmented-type)
       
       (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new)
       
       (production :short-new-expression (new :short-new-subexpression) short-new-expression-new)
       
       (production :full-new-subexpression ((:primary-expression normal)) full-new-subexpression-primary-expression)
       (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator)
       (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression)
       
       (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full)
       (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short)
       
       (production :member-operator ([ ]) member-operator-array-declaration)
       (production :member-operator ([ (:expression normal allow-in) ]) member-operator-array)
       (production :member-operator (\. :qualified-identifier) member-operator-property)
       (production :member-operator (\. :parenthesized-expression) member-operator-indirect-property)
       
       (production :arguments (\( \)) arguments-empty)
       (production :arguments (\( :argument-list \)) arguments-list)
       
       (production :argument-list ((:assignment-expression normal allow-in)) argument-list-one)
       (production :argument-list (:argument-list \, (:assignment-expression normal allow-in)) argument-list-more)
       
       
       (%subsection "Prefix Unary Operators")
       (production (:unary-expression :alpha) ((:postfix-expression :alpha)) unary-expression-postfix)
       (production (:unary-expression :alpha) (delete (:postfix-expression normal)) unary-expression-delete)
       (production (:unary-expression :alpha) (typeof (:unary-expression normal)) unary-expression-typeof)
       (production (:unary-expression :alpha) (eval (:unary-expression normal)) unary-expression-eval)
       (production (:unary-expression :alpha) (++ (:postfix-expression normal)) unary-expression-increment)
       (production (:unary-expression :alpha) (-- (:postfix-expression normal)) unary-expression-decrement)
       (production (:unary-expression :alpha) (+ (:unary-expression normal)) unary-expression-plus)
       (production (:unary-expression :alpha) (- (:unary-expression normal)) unary-expression-minus)
       (production (:unary-expression :alpha) (~ (:unary-expression normal)) unary-expression-bitwise-not)
       (production (:unary-expression :alpha) (! (:unary-expression normal)) unary-expression-logical-not)
       
       
       (%subsection "Multiplicative Operators")
       (production (:multiplicative-expression :alpha) ((:unary-expression :alpha)) multiplicative-expression-unary)
       (production (:multiplicative-expression :alpha) ((:multiplicative-expression :alpha) * (:unary-expression normal)) multiplicative-expression-multiply)
       (production (:multiplicative-expression :alpha) ((:multiplicative-expression :alpha) / (:unary-expression normal)) multiplicative-expression-divide)
       (production (:multiplicative-expression :alpha) ((:multiplicative-expression :alpha) % (:unary-expression normal)) multiplicative-expression-remainder)
       
       
       (%subsection "Additive Operators")
       (production (:additive-expression :alpha) ((:multiplicative-expression :alpha)) additive-expression-multiplicative)
       (production (:additive-expression :alpha) ((:additive-expression :alpha) + (:multiplicative-expression normal)) additive-expression-add)
       (production (:additive-expression :alpha) ((:additive-expression :alpha) - (:multiplicative-expression normal)) additive-expression-subtract)
       
       
       (%subsection "Bitwise Shift Operators")
       (production (:shift-expression :alpha) ((:additive-expression :alpha)) shift-expression-additive)
       (production (:shift-expression :alpha) ((:shift-expression :alpha) << (:additive-expression normal)) shift-expression-left)
       (production (:shift-expression :alpha) ((:shift-expression :alpha) >> (:additive-expression normal)) shift-expression-right-signed)
       (production (:shift-expression :alpha) ((:shift-expression :alpha) >>> (:additive-expression normal)) shift-expression-right-unsigned)
       
       
       (%subsection "Relational Operators")
       (exclude (:relational-expression initial no-in))
       (production (:relational-expression :alpha :beta) ((:shift-expression :alpha)) relational-expression-shift)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) < (:shift-expression normal)) relational-expression-less)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) > (:shift-expression normal)) relational-expression-greater)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) <= (:shift-expression normal)) relational-expression-less-or-equal)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) >= (:shift-expression normal)) relational-expression-greater-or-equal)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) instanceof (:shift-expression normal)) relational-expression-instanceof)
       (production (:relational-expression :alpha allow-in) ((:relational-expression :alpha allow-in) in (:shift-expression normal)) relational-expression-in)
       
       
       (%subsection "Equality Operators")
       (exclude (:equality-expression initial no-in))
       (production (:equality-expression :alpha :beta) ((:relational-expression :alpha :beta)) equality-expression-relational)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) == (:relational-expression normal :beta)) equality-expression-equal)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) != (:relational-expression normal :beta)) equality-expression-not-equal)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) === (:relational-expression normal :beta)) equality-expression-strict-equal)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) !== (:relational-expression normal :beta)) equality-expression-strict-not-equal)
       
       
       (%subsection "Binary Bitwise Operators")
       (exclude (:bitwise-and-expression initial no-in))
       (production (:bitwise-and-expression :alpha :beta) ((:equality-expression :alpha :beta)) bitwise-and-expression-equality)
       (production (:bitwise-and-expression :alpha :beta) ((:bitwise-and-expression :alpha :beta) & (:equality-expression normal :beta)) bitwise-and-expression-and)
       
       (exclude (:bitwise-xor-expression initial no-in))
       (production (:bitwise-xor-expression :alpha :beta) ((:bitwise-and-expression :alpha :beta)) bitwise-xor-expression-bitwise-and)
       (production (:bitwise-xor-expression :alpha :beta) ((:bitwise-xor-expression :alpha :beta) ^ (:bitwise-and-expression normal :beta)) bitwise-xor-expression-xor)
       
       (exclude (:bitwise-or-expression initial no-in))
       (production (:bitwise-or-expression :alpha :beta) ((:bitwise-xor-expression :alpha :beta)) bitwise-or-expression-bitwise-xor)
       (production (:bitwise-or-expression :alpha :beta) ((:bitwise-or-expression :alpha :beta) \| (:bitwise-xor-expression normal :beta)) bitwise-or-expression-or)
       
       
       (%subsection "Binary Logical Operators")
       (exclude (:logical-and-expression initial no-in))
       (production (:logical-and-expression :alpha :beta) ((:bitwise-or-expression :alpha :beta)) logical-and-expression-bitwise-or)
       (production (:logical-and-expression :alpha :beta) ((:logical-and-expression :alpha :beta) && (:bitwise-or-expression normal :beta)) logical-and-expression-and)
       
       (exclude (:logical-or-expression initial no-in))
       (production (:logical-or-expression :alpha :beta) ((:logical-and-expression :alpha :beta)) logical-or-expression-logical-and)
       (production (:logical-or-expression :alpha :beta) ((:logical-or-expression :alpha :beta) \|\| (:logical-and-expression normal :beta)) logical-or-expression-or)
       
       
       (%subsection "Conditional Operator")
       (exclude (:conditional-expression initial no-in))
       (production (:conditional-expression :alpha :beta) ((:logical-or-expression :alpha :beta)) conditional-expression-logical-or)
       (production (:conditional-expression :alpha :beta) ((:logical-or-expression :alpha :beta) ? (:assignment-expression normal :beta) \: (:assignment-expression normal :beta)) conditional-expression-conditional)
       
       (exclude (:non-assignment-expression initial no-in))
       (production (:non-assignment-expression :alpha :beta) ((:logical-or-expression :alpha :beta)) non-assignment-expression-logical-or)
       (production (:non-assignment-expression :alpha :beta) ((:logical-or-expression :alpha :beta) ? (:non-assignment-expression normal :beta) \: (:non-assignment-expression normal :beta)) non-assignment-expression-conditional)
       
       
       (%subsection "Assignment Operators")
       (exclude (:assignment-expression initial no-in))
       (production (:assignment-expression :alpha :beta) ((:conditional-expression :alpha :beta)) assignment-expression-conditional)
       (production (:assignment-expression :alpha :beta) ((:postfix-expression :alpha) = (:assignment-expression normal :beta)) assignment-expression-assignment)
       (production (:assignment-expression :alpha :beta) ((:postfix-expression :alpha) :compound-assignment (:assignment-expression normal :beta)) assignment-expression-compound)
       
       (production :compound-assignment (*=) compound-assignment-multiply)
       (production :compound-assignment (/=) compound-assignment-divide)
       (production :compound-assignment (%=) compound-assignment-remainder)
       (production :compound-assignment (+=) compound-assignment-add)
       (production :compound-assignment (-=) compound-assignment-subtract)
       (production :compound-assignment (<<=) compound-assignment-shift-left)
       (production :compound-assignment (>>=) compound-assignment-shift-right)
       (production :compound-assignment (>>>=) compound-assignment-shift-right-unsigned)
       (production :compound-assignment (&=) compound-assignment-and)
       (production :compound-assignment (^=) compound-assignment-or)
       (production :compound-assignment (\|=) compound-assignment-xor)
       
       
       (%subsection "Expressions")
       (exclude (:expression initial no-in))
       (production (:expression :alpha :beta) ((:assignment-expression :alpha :beta)) expression-assignment)
       (production (:expression :alpha :beta) ((:expression :alpha :beta) \, (:assignment-expression normal :beta)) expression-comma)
       
       (production :optional-expression ((:expression normal allow-in)) optional-expression-expression)
       (production :optional-expression () optional-expression-empty)
       
       
       (%subsection "Type Expressions")
       (exclude (:type-expression initial no-in))
       (production (:type-expression :alpha :beta) ((:non-assignment-expression :alpha :beta)) type-expression-non-assignment-expression)
       
       
       (%section "Statements")
       
       (grammar-argument :delta
                         top      ;top level in a package or program
                         class    ;top level in a class definition
                         block)   ;top level in a block
       (grammar-argument :omega
                         abbrev              ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                         abbrev-non-empty    ;optional semicolon as long as statement isn't empty
                         abbrev-no-short-if  ;optional semicolon, but statement must not end with an if without an else
                         full)               ;semicolon required at the end
       (grammar-argument :omega_3 abbrev abbrev-non-empty full)
       (grammar-argument :omega_2 abbrev full)
       
       (production (:statement :delta :omega_3) ((:code-statement :omega_3)) statement-code-statement)
       (production (:statement :delta :omega_3) ((:definition :delta :omega_3)) statement-definition)
       
       (production (:code-statement :omega) ((:empty-statement :omega)) code-statement-empty-statement)
       (production (:code-statement :omega) (:expression-statement (:semicolon :omega)) code-statement-expression-statement)
       (production (:code-statement :omega) ((:block block)) code-statement-block)
       (production (:code-statement :omega) ((:labeled-statement :omega)) code-statement-labeled-statement)
       (production (:code-statement :omega) ((:if-statement :omega)) code-statement-if-statement)
       (production (:code-statement :omega) (:switch-statement) code-statement-switch-statement)
       (production (:code-statement :omega) (:do-statement (:semicolon :omega)) code-statement-do-statement)
       (production (:code-statement :omega) ((:while-statement :omega)) code-statement-while-statement)
       (production (:code-statement :omega) ((:for-statement :omega)) code-statement-for-statement)
       (production (:code-statement :omega) (:continue-statement (:semicolon :omega)) code-statement-continue-statement)
       (production (:code-statement :omega) (:break-statement (:semicolon :omega)) code-statement-break-statement)
       (production (:code-statement :omega) (:return-statement (:semicolon :omega)) code-statement-return-statement)
       (production (:code-statement :omega) (:throw-statement (:semicolon :omega)) code-statement-throw-statement)
       (production (:code-statement :omega) (:try-statement) code-statement-try-statement)
       (production (:code-statement :omega) ((:import-statement :omega)) code-statement-import-statement)
       
       (production (:semicolon :omega) (\;) semicolon-semicolon)
       (production (:semicolon abbrev) () semicolon-abbrev)
       (production (:semicolon abbrev-non-empty) () semicolon-abbrev-non-empty)
       (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
       
       
       (%subsection "Empty Statement")
       (production (:empty-statement :omega) (\;) empty-statement-semicolon)
       (production (:empty-statement abbrev) () empty-statement-abbrev)
       (production (:empty-statement abbrev-no-short-if) () empty-statement-abbrev-no-short-if)
       
       
       (%subsection "Expression Statement")
       (production :expression-statement ((:expression initial allow-in)) expression-statement-expression)
       
       
       (%subsection "Block")
       (exclude (:block top))
       (production (:block :delta) ({ (:statements :delta) }) block-statements)
       
       (production (:statements :delta) ((:statement :delta abbrev)) statements-one)
       (production (:statements :delta) ((:statements-prefix :delta) (:statement :delta abbrev-non-empty)) statements-more)
       
       (production (:statements-prefix :delta) ((:statement :delta full)) statements-prefix-one)
       (production (:statements-prefix :delta) ((:statements-prefix :delta) (:statement :delta full)) statements-prefix-more)
       
       
       (%subsection "Labeled Statements")
       (production (:labeled-statement :omega) (:identifier \: (:code-statement :omega)) labeled-statement-label)
       
       
       (%subsection "If Statement")
       (production (:if-statement abbrev) (if :parenthesized-expression (:code-statement abbrev)) if-statement-if-then-abbrev)
       (production (:if-statement abbrev-non-empty) (if :parenthesized-expression (:code-statement abbrev-non-empty)) if-statement-if-then-abbrev-non-empty)
       (production (:if-statement full) (if :parenthesized-expression (:code-statement full)) if-statement-if-then-full)
       (production (:if-statement :omega) (if :parenthesized-expression (:code-statement abbrev-no-short-if)
                                              else (:code-statement :omega)) if-statement-if-then-else)
       
       
       (%subsection "Switch Statement")
       (production :switch-statement (switch :parenthesized-expression { }) switch-statement-empty)
       (production :switch-statement (switch :parenthesized-expression { :case-groups :last-case-group }) switch-statement-cases)
       
       (production :case-groups () case-groups-empty)
       (production :case-groups (:case-groups :case-group) case-groups-more)
       
       (production :case-group (:case-guards :case-statements-prefix) case-group-case-statements-prefix)
       
       (production :last-case-group (:case-guards :case-statements) last-case-group-case-statements)
       
       (production :case-guards (:case-guard) case-guards-one)
       (production :case-guards (:case-guards :case-guard) case-guards-more)
       
       (production :case-guard (case (:expression normal allow-in) \:) case-guard-case)
       (production :case-guard (default \:) case-guard-default)
       
       (production :case-statements ((:code-statement abbrev)) case-statements-one)
       (production :case-statements (:case-statements-prefix (:code-statement abbrev-non-empty)) case-statements-more)
       
       (production :case-statements-prefix ((:code-statement full)) case-statements-prefix-one)
       (production :case-statements-prefix (:case-statements-prefix (:code-statement full)) case-statements-prefix-more)
       
       
       (%subsection "Do-While Statement")
       (production :do-statement (do (:code-statement abbrev-non-empty) while :parenthesized-expression) do-statement-do-while)
       
       
       (%subsection "While Statement")
       (production (:while-statement :omega) (while :parenthesized-expression (:code-statement :omega)) while-statement-while)
       
       
       (%subsection "For Statements")
       (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                                (:code-statement :omega)) for-statement-c-style)
       (production (:for-statement :omega) (for \( :for-in-binding in (:expression normal allow-in) \) (:code-statement :omega)) for-statement-in)
       
       (production :for-initializer () for-initializer-empty)
       (production :for-initializer ((:expression normal no-in)) for-initializer-expression)
       (production :for-initializer (:variable-declaration-kind (:variable-declaration-list no-in)) for-initializer-variable-declaration)
       
       (production :for-in-binding ((:postfix-expression normal)) for-in-binding-expression)
       (production :for-in-binding (:variable-declaration-kind (:variable-declaration no-in)) for-in-binding-variable-declaration)
       
       
       (%subsection "Continue and Break Statements")
       (production :continue-statement (continue :optional-label) continue-statement-optional-label)
       
       (production :break-statement (break :optional-label) break-statement-optional-label)
       
       (production :optional-label () optional-label-default)
       (production :optional-label (:identifier) optional-label-identifier)
       
       
       (%subsection "Return Statement")
       (production :return-statement (return :optional-expression) return-statement-optional-expression)
       
       
       (%subsection "Throw Statement")
       (production :throw-statement (throw (:expression normal allow-in)) throw-statement-throw)
       
       
       (%subsection "Try Statement")
       (production :try-statement (try (:block block) :catch-clauses) try-statement-catch-clauses)
       (production :try-statement (try (:block block) :finally-clause) try-statement-finally-clause)
       (production :try-statement (try (:block block) :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
       
       (production :catch-clauses (:catch-clause) catch-clauses-one)
       (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
       
       (production :catch-clause (catch \( (:typed-identifier allow-in) \) (:block block)) catch-clause-block)
       
       (production :finally-clause (finally (:block block)) finally-clause-block)
       
       
       (%subsection "Import Statement")
       (production (:import-statement :omega) (import :import-list (:semicolon :omega)) import-statement-import)
       (production (:import-statement abbrev) (import :import-list (:block block)) import-statement-import-then-abbrev)
       (production (:import-statement abbrev-non-empty) (import :import-list (:block block)) import-statement-import-then-abbrev-non-empty)
       (production (:import-statement full) (import :import-list (:block block)) import-statement-import-then-full)
       (production (:import-statement :omega) (import :import-list (:block block) else (:code-statement :omega)) import-statement-import-then-else)
       
       (production :import-list (:import-item) import-list-one)
       (production :import-list (:import-list \, :import-item) import-list-more)
       
       (production :import-item (:import-source) import-item-import-source)
       (production :import-item (:identifier = :import-source) import-item-named-import-source)
       (production :import-item (protected :identifier = :import-source) import-item-protected-import-source)
       
       (production :import-source ((:non-assignment-expression normal no-in)) import-source-no-version)
       (production :import-source ((:non-assignment-expression normal no-in) \: :version) import-source-version)
       
       
       (%section "Definitions")
       (production (:definition :delta :omega_3) (:visibility (:global-definition :omega_3)) definition-global-definition)
       (production (:definition :delta :omega_3) ((:local-definition :delta :omega_3)) definition-local-definition)
       
       (production (:global-definition :omega_3) (:version-definition (:semicolon :omega_3)) global-definition-version-definition)
       (production (:global-definition :omega_3) (:variable-definition (:semicolon :omega_3)) global-definition-variable-definition)
       (production (:global-definition :omega_3) (:function-definition) global-definition-function-definition)
       (production (:global-definition :omega_3) ((:member-definition :omega_3)) global-definition-member-definition)
       (production (:global-definition :omega_3) (:class-definition) global-definition-class-definition)
       
       (production (:local-definition top :omega_3) (:version-definition (:semicolon :omega_3)) local-definition-top-version-definition)
       (production (:local-definition top :omega_3) (:variable-definition (:semicolon :omega_3)) local-definition-top-variable-definition)
       (production (:local-definition top :omega_3) (:function-definition) local-definition-top-function-definition)
       (production (:local-definition top :omega_3) (:class-definition) local-definition-top-class-definition)
       
       (production (:local-definition class :omega_3) (:variable-definition (:semicolon :omega_3)) local-definition-class-variable-definition)
       (production (:local-definition class :omega_3) (:function-definition) local-definition-class-function-definition)
       (production (:local-definition class :omega_3) ((:member-definition :omega_3)) local-definition-class-member-definition)
       (production (:local-definition class :omega_3) (:class-definition) local-definition-class-class-definition)
       
       (production (:local-definition block :omega_3) (:variable-definition (:semicolon :omega_3)) local-definition-block-variable-definition)
       (production (:local-definition block :omega_3) (:function-definition) local-definition-block-function-definition)
       
       
       (%subsection "Visibility Specifications")
       (production :visibility (private) visibility-private)
       (production :visibility (package) visibility-package)
       (production :visibility (public :versions-and-renames) visibility-public)
       
       (production :versions-and-renames () versions-and-renames-default)
       (production :versions-and-renames (< :version-ranges-and-aliases >) versions-and-renames-version-ranges-and-aliases)
       
       (production :version-ranges-and-aliases () version-ranges-and-aliases-none)
       (production :version-ranges-and-aliases (:version-ranges-and-aliases-prefix) version-ranges-and-aliases-some)
       
       (production :version-ranges-and-aliases-prefix (:version-range-and-alias) version-ranges-and-aliases-prefix-one)
       (production :version-ranges-and-aliases-prefix (:version-ranges-and-aliases-prefix \, :version-range-and-alias) version-ranges-and-aliases-prefix-more)
       
       (production :version-range-and-alias (:version-range) version-range-and-alias-version-range)
       (production :version-range-and-alias (:version-range \: :identifier) version-range-and-alias-version-range-and-alias)
       
       (production :version-range (:version) version-range-singleton)
       (production :version-range (:optional-version \.\. :optional-version) version-range-range)
       
       
       (%subsection "Versions")
       (production :optional-version (:version) optional-version-version)
       (production :optional-version () optional-version-none)
       
       (production :version ($number) version-number)
       (production :version ($string) version-string)
       
       
       (%subsection "Version Definition")
       (production :version-definition (version :version) version-definition-version)
       (production :version-definition (version :version > :version-list) version-definition-ranked-version)
       
       (production :version-list (:version) version-list-one)
       (production :version-list (:version-list \, :version) version-list-more)
       
       
       (%subsection "Variable Definition")
       (production :variable-definition (:variable-declaration-kind (:variable-declaration-list allow-in)) variable-definition-declaration)
       
       (production :variable-declaration-kind (var) variable-declaration-kind-var)
       (production :variable-declaration-kind (const) variable-declaration-kind-const)
       
       (production (:variable-declaration-list :beta) ((:variable-declaration :beta)) variable-declaration-list-one)
       (production (:variable-declaration-list :beta) ((:variable-declaration-list :beta) \, (:variable-declaration :beta)) variable-declaration-list-more)
       
       (production (:variable-declaration :beta) ((:typed-identifier :beta) (:variable-initializer :beta)) variable-declaration-initializer)
       
       (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier)
       (production (:typed-identifier :beta) ((:type-expression normal :beta) :identifier) typed-identifier-type-and-identifier)
       
       (production (:variable-initializer :beta) () variable-initializer-empty)
       (production (:variable-initializer :beta) (= (:assignment-expression normal :beta)) variable-initializer-assignment-expression)
       
       
       (%subsection "Function Definition")
       (production :function-definition (:named-function) function-definition-named-function)
       
       (production :anonymous-function (function :function-signature (:block block)) anonymous-function-signature-and-body)
       
       (production :named-function (function :identifier :function-signature (:block block)) named-function-signature-and-body)
       
       (production :function-signature (:parameter-signature :result-signature) function-signature-parameter-and-result-signatures)
       
       (production :parameter-signature (\( :parameters \)) parameter-signature-parameters)
       
       (production :parameters () parameters-none)
       (production :parameters (\.\.\. :identifier) parameters-rest)
       (production :parameters (:parameters-prefix) parameters-some)
       (production :parameters (:parameters-prefix \, \.\.\. :identifier) parameters-some-and-rest)
       
       (production :parameters-prefix ((:typed-identifier allow-in)) parameters-prefix-one)
       (production :parameters-prefix (:parameters-prefix \, (:typed-identifier allow-in)) parameters-prefix-more)
       
       (production :result-signature () result-signature-none)
       (production :result-signature ((:type-expression initial allow-in)) result-signature-type-expression)
       
       
       (%subsection "Class Member Definitions")
       (production (:member-definition :omega_3) (:field-definition (:semicolon :omega_3)) member-definition-field-definition)
       (production (:member-definition :omega_3) ((:method-definition :omega_3)) member-definition-method-definition)
       (production (:member-definition :omega_3) (:constructor-definition) member-definition-constructor-definition)
       
       (production :field-definition (field (:variable-declaration-list allow-in)) field-definition-declaration)
       
       (production (:method-definition :omega_3) (:concrete-method-definition) method-definition-concrete-method-definition)
       (production (:method-definition :omega_3) ((:abstract-method-definition :omega_3)) method-definition-abstract-method-definition)
       
       (production :concrete-method-definition (:method-prefix :identifier :function-signature (:block block)) concrete-method-definition-signature-and-body)
       
       (production (:abstract-method-definition :omega_3) (:method-prefix :identifier :function-signature (:semicolon :omega_3)) abstract-method-definition-signature)
       
       (production :method-prefix (method) method-prefix-method)
       (production :method-prefix (override method) method-prefix-override-method)
       (production :method-prefix (final method) method-prefix-final-method)
       (production :method-prefix (final override method) method-prefix-final-override-method)
       
       (production :constructor-definition (constructor :identifier :parameter-signature (:block block)) constructor-definition-signature-and-body)
       
       
       (%section "Class Definition")
       (production :class-definition (class :identifier :superclasses (:block class)) class-definition-normal)
       (production :class-definition (class extends (:type-expression initial allow-in) (:block class)) class-definition-augmented)
       
       (production :superclasses () superclasses-none)
       (production :superclasses (extends (:type-expression initial allow-in)) superclasses-one)
       
       
       (%section "Programs")
       
       (production :program ((:statements top)) program-statements)
       )))
  
  (defparameter *jg* (world-grammar *jw* 'code-grammar)))



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
    
    (let ((bins (make-array 6 :initial-element nil))
          (terminals (grammar-terminals grammar)))
      (setf (svref bins 2) (list '\# '&&= '-> '@ '^^ '^^= '\|\|=))
      (setf (svref bins 4) (list 'abstract 'debugger 'enum 'export 'goto 'implements 'native 'package 'protected 'static 'super 'synchronized
                                 'throws 'transient 'volatile 'with))
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
(let ((*visible-modes* nil))
  (depict-rtf-to-local-file
   "JS20.rtf"
   #'(lambda (markup-stream)
       (depict-js-terminals markup-stream *jg*)
       (depict-world-commands markup-stream *jw*))))

(let ((*visible-modes* nil))
  (depict-html-to-local-file
   "JS20.html"
   #'(lambda (markup-stream)
       (depict-js-terminals markup-stream *jg*)
       (depict-world-commands markup-stream *jw*))
   "JavaScript 2.0 Grammar"))

(with-local-output (s "JS20.txt") (print-grammar *jg* s))
|#

(length (grammar-states *jg*))
