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
     '((grammar code-grammar :lalr-1 :program)
       
       
       (%section "Expressions")
       (grammar-argument :beta allow-in no-in)
       
       (%subsection "Identifiers")
       (production :identifier ($identifier) identifier-identifier)
       ;(production :identifier (const) identifier-const)
       (production :identifier (version) identifier-version)
       (production :identifier (override) identifier-override)
       ;(production :identifier (field) identifier-field)
       (production :identifier (method) identifier-method)
       (production :identifier (getter) identifier-getter)
       (production :identifier (setter) identifier-setter)
       (production :identifier (traditional) identifier-traditional)
       (production :identifier (constructor) identifier-constructor)
       
       (production :qualified-identifier (:identifier) qualified-identifier-identifier)
       (production :qualified-identifier (:qualified-identifier \:\: :identifier) qualified-identifier-multi-level)
       (production :qualified-identifier (:parenthesized-expression \:\: :identifier) qualified-identifier-parenthesized-expression)
       
       
       (%subsection "Primary Expressions")
       (production :primary-expression (null) primary-expression-null)
       (production :primary-expression (true) primary-expression-true)
       (production :primary-expression (false) primary-expression-false)
       (production :primary-expression ($number) primary-expression-number)
       (production :primary-expression ($string) primary-expression-string)
       (production :primary-expression (this) primary-expression-this)
       (production :primary-expression (super) primary-expression-super)
       (production :primary-expression (:qualified-identifier) primary-expression-qualified-identifier)
       (production :primary-expression ($regular-expression) primary-expression-regular-expression)
       (production :primary-expression (:parenthesized-expression) primary-expression-parenthesized-expression)
       (production :primary-expression (:array-literal) primary-expression-array-literal)
       (production :primary-expression (:object-literal) primary-expression-object-literal)
       (production :primary-expression (:function-expression) primary-expression-function-expression)
       
       (production :parenthesized-expression (\( (:expression allow-in) \)) parenthesized-expression-expression)
       
       
       (%subsection "Function Expressions")
       (production :function-expression (:anonymous-function) function-expression-anonymous-function)
       (production :function-expression (:named-function) function-expression-named-function)
       
       
       (%subsection "Object Literals")
       (production :object-literal (\{ \}) object-literal-empty)
       (production :object-literal (\{ :field-list \}) object-literal-list)
       
       (production :field-list (:literal-field) field-list-one)
       (production :field-list (:field-list \, :literal-field) field-list-more)
       
       (production :literal-field (:qualified-identifier \: (:assignment-expression allow-in)) literal-field-assignment-expression)
       
       
       (%subsection "Array Literals")
       (production :array-literal ([ ]) array-literal-empty)
       (production :array-literal ([ :element-list ]) array-literal-list)
       
       (production :element-list (:literal-element) element-list-one)
       (production :element-list (:element-list \, :literal-element) element-list-more)
       
       (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
       
       
       (%subsection "Postfix Unary Operators")
       (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression)
       (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression)
       
       (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression)
       (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression)
       (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator)
       (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call)
       (production :full-postfix-expression (:postfix-expression ++) full-postfix-expression-increment)
       (production :full-postfix-expression (:postfix-expression --) full-postfix-expression-decrement)
       
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
       (production :argument-list (:argument-list-prefix) argument-list-some)
       
       (production :argument-list-prefix ((:assignment-expression allow-in)) argument-list-prefix-one)
       (production :argument-list-prefix (:argument-list-prefix \, (:assignment-expression allow-in)) argument-list-prefix-more)
       
       
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
       (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ *) bitwise-xor-expression-null)
       (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ ?) bitwise-xor-expression-undefined)
       
       (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor)
       (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| (:bitwise-xor-expression :beta)) bitwise-or-expression-or)
       (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| *) bitwise-or-expression-null)
       (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| ?) bitwise-or-expression-undefined)
       
       
       (%subsection "Binary Logical Operators")
       (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or)
       (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and)
       
       (production (:logical-or-expression :beta) ((:logical-and-expression :beta)) logical-or-expression-logical-and)
       (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-and-expression :beta)) logical-or-expression-or)
       
       
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
       (production :compound-assignment (&=) compound-assignment-and)
       (production :compound-assignment (^=) compound-assignment-or)
       (production :compound-assignment (\|=) compound-assignment-xor)
       
       
       (%subsection "Expressions")
       (production (:expression :beta) ((:assignment-expression :beta)) expression-assignment)
       (production (:expression :beta) ((:expression :beta) \, (:assignment-expression :beta)) expression-comma)
       
       (production :optional-expression ((:expression allow-in)) optional-expression-expression)
       (production :optional-expression () optional-expression-empty)
       
       
       (%subsection "Type Expressions")
       (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression)
       
       
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
       (production (:code-statement :omega) ((:with-statement :omega)) code-statement-with-statement)
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
       (production :expression-statement ((:- function {) (:expression allow-in)) expression-statement-expression)
       
       
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
       
       (production :case-guard (case (:expression allow-in) \:) case-guard-case)
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
       (production (:for-statement :omega) (for \( :for-in-binding in (:expression allow-in) \) (:code-statement :omega)) for-statement-in)
       
       (production :for-initializer () for-initializer-empty)
       (production :for-initializer ((:expression no-in)) for-initializer-expression)
       (production :for-initializer (:variable-definition-kind (:variable-binding-list no-in)) for-initializer-variable-definition)
       
       (production :for-in-binding (:postfix-expression) for-in-binding-expression)
       (production :for-in-binding (:variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
       
       
       (%subsection "With Statement")
       (production (:with-statement :omega) (with :parenthesized-expression (:code-statement :omega)) with-statement-with)
       
       
       (%subsection "Continue and Break Statements")
       (production :continue-statement (continue :optional-label) continue-statement-optional-label)
       
       (production :break-statement (break :optional-label) break-statement-optional-label)
       
       (production :optional-label () optional-label-default)
       (production :optional-label (:identifier) optional-label-identifier)
       
       
       (%subsection "Return Statement")
       (production :return-statement (return :optional-expression) return-statement-optional-expression)
       
       
       (%subsection "Throw Statement")
       (production :throw-statement (throw (:expression allow-in)) throw-statement-throw)
       
       
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
       
       (production :import-source ((:non-assignment-expression no-in)) import-source-no-version)
       (production :import-source ((:non-assignment-expression no-in) \: :version) import-source-version)
       
       
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
       
       
       (%subsection "Variable Definition")
       (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition)
       
       (production :variable-definition-kind (var) variable-definition-kind-var)
       (production :variable-definition-kind (const) variable-definition-kind-const)
       
       (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one)
       (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more)
       
       (production (:variable-binding :beta) ((:typed-identifier :beta) (:variable-initializer :beta)) variable-binding-initializer)
       
       (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier)
       (production (:typed-identifier :beta) ((:type-expression :beta) :identifier) typed-identifier-type-and-identifier)
       
       (production (:variable-initializer :beta) () variable-initializer-empty)
       (production (:variable-initializer :beta) (= (:assignment-expression :beta)) variable-initializer-assignment-expression)
       
       
       (%subsection "Function Definition")
       (production :function-definition (:named-function) function-definition-named-function)
       (production :function-definition (getter :named-function) function-definition-getter-function)
       (production :function-definition (setter :named-function) function-definition-setter-function)
       (production :function-definition (:traditional-function) function-definition-traditional-function)
       
       (production :anonymous-function (function :function-signature (:block block)) anonymous-function-signature-and-body)
       
       (production :named-function (function :identifier :function-signature (:block block)) named-function-signature-and-body)
       
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
       
       (production :optional-parameter ((:typed-identifier allow-in) =) optional-parameter-default)
       (production :optional-parameter ((:typed-identifier allow-in) = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
       
       (production :rest-parameter (\.\.\.) rest-parameter-none)
       (production :rest-parameter (\.\.\. :identifier) rest-parameter-identifier)
       
       (production :result-signature () result-signature-none)
       (production :result-signature ((:- {) (:type-expression allow-in)) result-signature-type-expression)
       
       (production :traditional-function (traditional function :identifier \( :traditional-parameter-list \) (:block block)) traditional-function-signature-and-body)
       
       (production :traditional-parameter-list () traditional-parameter-list-none)
       (production :traditional-parameter-list (:traditional-parameter-list-prefix) traditional-parameter-list-some)
       
       (production :traditional-parameter-list-prefix (:identifier) traditional-parameter-list-prefix-one)
       (production :traditional-parameter-list-prefix (:traditional-parameter-list-prefix \, :identifier) traditional-parameter-list-prefix-more)
       
       
       (%subsection "Class Member Definitions")
       (production (:member-definition :omega_3) (:field-definition (:semicolon :omega_3)) member-definition-field-definition)
       (production (:member-definition :omega_3) ((:method-definition :omega_3)) member-definition-method-definition)
       (production (:member-definition :omega_3) (:constructor-definition) member-definition-constructor-definition)
       
       (production :field-definition (field (:variable-binding-list allow-in)) field-definition-variable-binding-list)
       
       (production (:method-definition :omega_3) (:concrete-method-definition) method-definition-concrete-method-definition)
       (production (:method-definition :omega_3) ((:abstract-method-definition :omega_3)) method-definition-abstract-method-definition)
       
       (production :concrete-method-definition (:method-prefix :identifier :function-signature (:block block)) concrete-method-definition-signature-and-body)
       
       (production (:abstract-method-definition :omega_3) (:method-prefix :identifier :function-signature (:semicolon :omega_3)) abstract-method-definition-signature)
       
       (production :method-prefix (:method-kind) method-prefix-method)
       (production :method-prefix (override :method-kind) method-prefix-override-method)
       (production :method-prefix (final :method-kind) method-prefix-final-method)
       (production :method-prefix (final override :method-kind) method-prefix-final-override-method)
       
       (production :method-kind (method) method-kind-method)
       (production :method-kind (getter method) method-kind-getter-method)
       (production :method-kind (setter method) method-kind-setter-method)
       
       (production :constructor-definition (constructor :constructor-name :parameter-signature (:block block)) constructor-definition-signature-and-body)
       
       (production :constructor-name (new) constructor-name-new)
       (production :constructor-name (:identifier) constructor-name-identifier)
       
       
       (%section "Class Definition")
       (production :class-definition (class :identifier :superclasses (:block class)) class-definition-normal)
       (production :class-definition (class extends (:type-expression allow-in) (:block class)) class-definition-augmented)
       
       (production :superclasses () superclasses-none)
       (production :superclasses (extends (:type-expression allow-in)) superclasses-one)
       
       
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
(depict-rtf-to-local-file
 ";JS20;ParserGrammar.rtf"
 "JavaScript 2.0 Parser Grammar"
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg*)
     (depict-world-commands markup-stream *jw* :visible-semantics nil)))

(depict-html-to-local-file
 ";JS20;ParserGrammar.html"
 "JavaScript 2.0 Parser Grammar"
 t
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg*)
     (depict-world-commands markup-stream *jw* :visible-semantics nil)))

(with-local-output (s ";JS20;ParserGrammar.txt") (print-grammar *jg* s))
|#

(length (grammar-states *jg*))
