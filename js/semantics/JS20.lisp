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
       (grammar-argument :alpha normal initial no-in)
       (grammar-argument :alpha_2 normal initial)
       (grammar-argument :alpha_e normal no-in)
       
       (%subsection "Primary Expressions")
       (production (:primary-expression :alpha_2) (:simple-expression) primary-expression-simple-expression)
       (production (:primary-expression normal) (:function-expression) primary-expression-function-expression)
       (production (:primary-expression normal) (:object-literal) primary-expression-object-literal)
       
       (production :simple-expression (this) simple-expression-this)
       (production :simple-expression (null) simple-expression-null)
       (production :simple-expression (true) simple-expression-true)
       (production :simple-expression (false) simple-expression-false)
       (production :simple-expression ($number) simple-expression-number)
       (production :simple-expression ($string) simple-expression-string)
       (production :simple-expression ($identifier) simple-expression-identifier)
       (production :simple-expression ($regular-expression) simple-expression-regular-expression)
       (production :simple-expression (:parenthesized-expression) simple-expression-parenthesized-expression)
       (production :simple-expression (:array-literal) simple-expression-array-literal)
       
       (production :parenthesized-expression (\( (:expression normal) \)) parenthesized-expression-expression)
       
       
       (%subsection "Function Expressions")
       (production :function-expression (:anonymous-function) function-expression-anonymous-function)
       (production :function-expression (:named-function) function-expression-named-function)
       
       
       (%subsection "Object Literals")
       (production :object-literal (\{ \}) object-literal-empty)
       (production :object-literal (\{ :field-list \}) object-literal-list)
       
       (production :field-list (:literal-field) field-list-one)
       (production :field-list (:field-list \, :literal-field) field-list-more)
       
       (production :literal-field ($identifier \: (:assignment-expression normal)) literal-field-assignment-expression)
       
       
       (%subsection "Array Literals")
       (production :array-literal ([ ]) array-literal-empty)
       (production :array-literal ([ :element-list ]) array-literal-list)
       
       (production :element-list (:literal-element) element-list-one)
       (production :element-list (:element-list \, :literal-element) element-list-more)
       
       (production :literal-element ((:assignment-expression normal)) literal-element-assignment-expression)
       
       
       (%subsection "Left-Side Expressions")
       (grammar-argument :chi allow-calls no-calls)
       
       (production (:member-expression :alpha_2 no-calls) ((:primary-expression :alpha_2)) member-expression-primary-expression)
       (production (:member-expression :alpha_2 allow-calls) ((:member-expression :alpha_2 no-calls) :arguments) call-expression-call-member-expression)
       (production (:member-expression :alpha_2 allow-calls) ((:member-expression :alpha_2 allow-calls) :arguments) call-expression-call-call-expression)
       (production (:member-expression :alpha_2 :chi) ((:member-expression :alpha_2 :chi) [ ]) member-expression-array-declaration)
       (production (:member-expression :alpha_2 :chi) ((:member-expression :alpha_2 :chi) [ (:expression normal) ]) member-expression-array)
       (production (:member-expression :alpha_2 :chi) ((:member-expression :alpha_2 :chi) \. $identifier) member-expression-property)
       (production (:member-expression :alpha_2 :chi) ((:member-expression :alpha_2 :chi) \. :parenthesized-expression) member-expression-indirect-property)
       (production (:member-expression :alpha_2 no-calls) (new (:member-expression normal no-calls) :arguments) member-expression-new)
       
       (production (:new-expression :alpha_2) ((:member-expression :alpha_2 no-calls)) new-expression-member-expression)
       (production (:new-expression :alpha_2) (new (:new-expression normal)) new-expression-new)
       
       (production :arguments (\( \)) arguments-empty)
       (production :arguments (\( :argument-list \)) arguments-list)
       
       (production :argument-list ((:assignment-expression normal)) argument-list-one)
       (production :argument-list (:argument-list \, (:assignment-expression normal)) argument-list-more)
       
       (production (:left-side-expression :alpha_2) ((:new-expression :alpha_2)) left-side-expression-new-expression)
       (production (:left-side-expression :alpha_2) ((:member-expression :alpha_2 allow-calls)) left-side-expression-call-expression)
       
       
       (%subsection "Postfix Expressions")
       (production (:postfix-expression :alpha_2) ((:left-side-expression :alpha_2)) postfix-expression-left-side-expression)
       (production (:postfix-expression :alpha_2) ((:left-side-expression :alpha_2) ++) postfix-expression-increment)
       (production (:postfix-expression :alpha_2) ((:left-side-expression :alpha_2) --) postfix-expression-decrement)
       
       
       (%subsection "Unary Operators")
       (production (:unary-expression :alpha_2) ((:postfix-expression :alpha_2)) unary-expression-postfix)
       (production (:unary-expression :alpha_2) (delete (:left-side-expression normal)) unary-expression-delete)
       (production (:unary-expression :alpha_2) (void (:unary-expression normal)) unary-expression-void)
       (production (:unary-expression :alpha_2) (typeof (:unary-expression normal)) unary-expression-typeof)
       (production (:unary-expression :alpha_2) (++ (:left-side-expression normal)) unary-expression-increment)
       (production (:unary-expression :alpha_2) (-- (:left-side-expression normal)) unary-expression-decrement)
       (production (:unary-expression :alpha_2) (+ (:unary-expression normal)) unary-expression-plus)
       (production (:unary-expression :alpha_2) (- (:unary-expression normal)) unary-expression-minus)
       (production (:unary-expression :alpha_2) (~ (:unary-expression normal)) unary-expression-bitwise-not)
       (production (:unary-expression :alpha_2) (! (:unary-expression normal)) unary-expression-logical-not)
       
       
       (%subsection "Multiplicative Operators")
       (production (:multiplicative-expression :alpha_2) ((:unary-expression :alpha_2)) multiplicative-expression-unary)
       (production (:multiplicative-expression :alpha_2) ((:multiplicative-expression :alpha_2) * (:unary-expression normal)) multiplicative-expression-multiply)
       (production (:multiplicative-expression :alpha_2) ((:multiplicative-expression :alpha_2) / (:unary-expression normal)) multiplicative-expression-divide)
       (production (:multiplicative-expression :alpha_2) ((:multiplicative-expression :alpha_2) % (:unary-expression normal)) multiplicative-expression-remainder)
       
       
       (%subsection "Additive Operators")
       (production (:additive-expression :alpha_2) ((:multiplicative-expression :alpha_2)) additive-expression-multiplicative)
       (production (:additive-expression :alpha_2) ((:additive-expression :alpha_2) + (:multiplicative-expression normal)) additive-expression-add)
       (production (:additive-expression :alpha_2) ((:additive-expression :alpha_2) - (:multiplicative-expression normal)) additive-expression-subtract)
       
       
       (%subsection "Bitwise Shift Operators")
       (production (:shift-expression :alpha_2) ((:additive-expression :alpha_2)) shift-expression-additive)
       (production (:shift-expression :alpha_2) ((:shift-expression :alpha_2) << (:additive-expression normal)) shift-expression-left)
       (production (:shift-expression :alpha_2) ((:shift-expression :alpha_2) >> (:additive-expression normal)) shift-expression-right-signed)
       (production (:shift-expression :alpha_2) ((:shift-expression :alpha_2) >>> (:additive-expression normal)) shift-expression-right-unsigned)
       
       
       (%subsection "Relational Operators")
       (production (:relational-expression normal) ((:shift-expression normal)) relational-expression-shift-normal)
       (production (:relational-expression initial) ((:shift-expression initial)) relational-expression-shift-initial)
       (production (:relational-expression no-in) ((:shift-expression normal)) relational-expression-shift-no-in)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) < (:shift-expression normal)) relational-expression-less)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) > (:shift-expression normal)) relational-expression-greater)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) <= (:shift-expression normal)) relational-expression-less-or-equal)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) >= (:shift-expression normal)) relational-expression-greater-or-equal)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) instanceof (:shift-expression normal)) relational-expression-instanceof)
       (production (:relational-expression normal) ((:relational-expression normal) in (:shift-expression normal)) relational-expression-in-normal)
       (production (:relational-expression initial) ((:relational-expression initial) in (:shift-expression normal)) relational-expression-in-initial)
       
       
       (%subsection "Equality Operators")
       (production (:equality-expression :alpha) ((:relational-expression :alpha)) equality-expression-relational)
       (production (:equality-expression :alpha) ((:equality-expression :alpha) == (:relational-expression :alpha)) equality-expression-equal)
       (production (:equality-expression :alpha) ((:equality-expression :alpha) != (:relational-expression :alpha)) equality-expression-not-equal)
       (production (:equality-expression :alpha) ((:equality-expression :alpha) === (:relational-expression :alpha)) equality-expression-strict-equal)
       (production (:equality-expression :alpha) ((:equality-expression :alpha) !== (:relational-expression :alpha)) equality-expression-strict-not-equal)
       
       
       (%subsection "Binary Bitwise Operators")
       (production (:bitwise-and-expression :alpha) ((:equality-expression :alpha)) bitwise-and-expression-equality)
       (production (:bitwise-and-expression :alpha) ((:bitwise-and-expression :alpha) & (:equality-expression :alpha)) bitwise-and-expression-and)
       
       (production (:bitwise-xor-expression :alpha) ((:bitwise-and-expression :alpha)) bitwise-xor-expression-bitwise-and)
       (production (:bitwise-xor-expression :alpha) ((:bitwise-xor-expression :alpha) ^ (:bitwise-and-expression :alpha)) bitwise-xor-expression-xor)
       
       (production (:bitwise-or-expression :alpha) ((:bitwise-xor-expression :alpha)) bitwise-or-expression-bitwise-xor)
       (production (:bitwise-or-expression :alpha) ((:bitwise-or-expression :alpha) \| (:bitwise-xor-expression :alpha)) bitwise-or-expression-or)
       
       
       (%subsection "Binary Logical Operators")
       (production (:logical-and-expression :alpha) ((:bitwise-or-expression :alpha)) logical-and-expression-bitwise-or)
       (production (:logical-and-expression :alpha) ((:logical-and-expression :alpha) && (:bitwise-or-expression :alpha)) logical-and-expression-and)
       
       (production (:logical-or-expression :alpha) ((:logical-and-expression :alpha)) logical-or-expression-logical-and)
       (production (:logical-or-expression :alpha) ((:logical-or-expression :alpha) \|\| (:logical-and-expression :alpha)) logical-or-expression-or)
       
       
       (%subsection "Conditional Operator")
       (production (:conditional-expression :alpha) ((:logical-or-expression :alpha)) conditional-expression-logical-or)
       (production (:conditional-expression :alpha) ((:logical-or-expression :alpha) ? (:assignment-expression :alpha) \: (:assignment-expression :alpha)) conditional-expression-conditional)
       
       
       (%subsection "Assignment Operators")
       (production (:assignment-expression :alpha) ((:conditional-expression :alpha)) assignment-expression-conditional)
       (production (:assignment-expression :alpha) ((:assignment-left-side :alpha) = (:assignment-expression :alpha)) assignment-expression-assignment)
       (production (:assignment-expression :alpha) ((:assignment-left-side :alpha) :compound-assignment (:assignment-expression :alpha)) assignment-expression-compound)
       
       (production (:assignment-left-side normal) ((:left-side-expression normal)) assignment-left-side-normal)
       (production (:assignment-left-side initial) ((:left-side-expression initial)) assignment-left-side-initial)
       (production (:assignment-left-side no-in) ((:left-side-expression normal)) assignment-left-side-no-in)
       
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
       (production (:expression :alpha) ((:assignment-expression :alpha)) expression-assignment)
       (production (:expression :alpha) ((:expression :alpha) \, (:assignment-expression :alpha)) expression-comma)
       
       (production :optional-expression ((:expression normal)) optional-expression-expression)
       (production :optional-expression () optional-expression-empty)
       
       
       (%subsection "Type Expressions")
       (production (:type-expression :alpha_e) ((:logical-or-expression :alpha_e)) type-expression-logical-or)
       (production (:type-expression :alpha_e) ((:logical-or-expression :alpha_e) ? (:type-expression :alpha_e) \: (:type-expression :alpha_e)) type-expression-conditional)
       
       ;(production (:optional-type-expression :alpha_e) () optional-type-expression-empty)
       ;(production (:optional-type-expression :alpha_e) ((:type-expression :alpha_e)) optional-type-expression-type-expression)
       
       
       (%section "Statements")
       
       (grammar-argument :omega
                         abbrev ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                         abbrev-non-empty ;optional semicolon as long as statement isn't empty
                         abbrev-no-short-if ;optional semicolon, but statement must not end with an if without an else
                         full) ;semicolon required at the end
       (grammar-argument :omega_3 abbrev abbrev-non-empty full)
       
       (production (:block-statement :omega_3) ((:statement :omega_3)) block-statement-statement)
       (production (:block-statement :omega_3) ((:local-definition :omega_3)) block-statement-local-definition)
       
       (production (:statement :omega) ((:empty-statement :omega)) statement-empty-statement)
       (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement)
       (production (:statement :omega) (:block) statement-block)
       (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement)
       (production (:statement :omega) ((:if-statement :omega)) statement-if-statement)
       (production (:statement :omega) (:switch-statement) statement-switch-statement)
       (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement)
       (production (:statement :omega) ((:while-statement :omega)) statement-while-statement)
       (production (:statement :omega) ((:for-statement :omega)) statement-for-statement)
       (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement)
       (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement)
       (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement)
       (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement)
       (production (:statement :omega) (:try-statement) statement-try-statement)
       
       (production (:semicolon :omega) (\;) semicolon-semicolon)
       (production (:semicolon abbrev) () semicolon-abbrev)
       (production (:semicolon abbrev-non-empty) () semicolon-abbrev-non-empty)
       (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
       
       
       (%subsection "Empty Statement")
       (production (:empty-statement :omega) (\;) empty-statement-semicolon)
       (production (:empty-statement abbrev) () empty-statement-abbrev)
       (production (:empty-statement abbrev-no-short-if) () empty-statement-abbrev-no-short-if)
       
       
       (%subsection "Expression Statement")
       (production :expression-statement ((:expression initial)) expression-statement-expression)
       
       
       (%subsection "Block")
       (production :block ({ :block-statements }) block-block-statements)
       
       (production :block-statements ((:block-statement abbrev)) block-statements-one)
       (production :block-statements (:block-statements-prefix (:block-statement abbrev-non-empty)) block-statements-more)
       
       (production :block-statements-prefix ((:block-statement full)) block-statements-prefix-one)
       (production :block-statements-prefix (:block-statements-prefix (:block-statement full)) block-statements-prefix-more)
       
       
       (%subsection "Labeled Statements")
       (production (:labeled-statement :omega) ($identifier \: (:statement :omega)) labeled-statement-label)
       
       
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
       
       (production :case-guard (case (:expression normal) \:) case-guard-case)
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
       (production (:for-statement :omega) (for \( :for-in-binding in (:expression normal) \) (:statement :omega)) for-statement-in)
       
       (production :for-initializer () for-initializer-empty)
       (production :for-initializer ((:expression no-in)) for-initializer-expression)
       (production :for-initializer (:variable-declaration-kind (:variable-declaration-list no-in)) for-initializer-variable-declaration)
       
       (production :for-in-binding ((:left-side-expression normal)) for-in-binding-expression)
       (production :for-in-binding (:variable-declaration-kind (:variable-declaration no-in)) for-in-binding-variable-declaration)
       
       
       (%subsection "Continue and Break Statements")
       (production :continue-statement (continue :optional-label) continue-statement-optional-label)
       
       (production :break-statement (break :optional-label) break-statement-optional-label)
       
       (production :optional-label () optional-label-default)
       (production :optional-label ($identifier) optional-label-identifier)
       
       
       (%subsection "Return Statement")
       (production :return-statement (return :optional-expression) return-statement-optional-expression)
       
       
       (%subsection "Throw Statement")
       (production :throw-statement (throw (:expression normal)) throw-statement-throw)
       
       
       (%subsection "Try Statement")
       (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses)
       (production :try-statement (try :block :finally-clause) try-statement-finally-clause)
       (production :try-statement (try :block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
       
       (production :catch-clauses (:catch-clause) catch-clauses-one)
       (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
       
       (production :catch-clause (catch \( $identifier (:type-declaration normal) \) :block) catch-clause-block)
       
       (production :finally-clause (finally :block) finally-clause-block)
       
       
       (%section "Local Definitions")
       
       (production (:local-definition :omega_3) (:variable-definition (:semicolon :omega_3)) local-definition-variable-definition)
       (production (:local-definition :omega_3) (:function-definition) local-definition-function-definition)
       
       
       (%subsection "Variable Definition")
       (production :variable-definition (:variable-declaration-kind (:variable-declaration-list normal)) variable-definition-declaration)
       
       (production :variable-declaration-kind (var) variable-declaration-kind-var)
       (production :variable-declaration-kind (const) variable-declaration-kind-const)
       
       (production (:variable-declaration-list :alpha_e) ((:variable-declaration :alpha_e)) variable-declaration-list-one)
       (production (:variable-declaration-list :alpha_e) ((:variable-declaration-list :alpha_e) \, (:variable-declaration :alpha_e)) variable-declaration-list-more)
       
       (production (:variable-declaration :alpha_e) ($identifier (:type-declaration :alpha_e) (:variable-initializer :alpha_e)) variable-declaration-initializer)
       
       (production (:type-declaration :alpha_e) () type-declaration-empty)
       (production (:type-declaration :alpha_e) (\: (:type-expression :alpha_e)) type-declaration-type-expression)
       
       (production (:variable-initializer :alpha_e) () variable-initializer-empty)
       (production (:variable-initializer :alpha_e) (= (:assignment-expression :alpha_e)) variable-initializer-assignment-expression)
       
       
       (%subsection "Function Definition")
       (production :function-definition (:named-function) function-definition-named-function)
       
       (production :anonymous-function (function :formal-parameters-and-body) anonymous-function-formal-parameters-and-body)
       
       (production :named-function (function $identifier :formal-parameters-and-body) named-function-formal-parameters-and-body)
       
       (production :formal-parameters-and-body (\( :formal-parameters \) (:type-declaration normal) :block) formal-parameters-and-body)
       
       (production :formal-parameters () formal-parameters-none)
       (production :formal-parameters (:formal-parameters-prefix) formal-parameters-some)
       
       (production :formal-parameters-prefix (:formal-parameter) formal-parameters-prefix-one)
       (production :formal-parameters-prefix (:formal-parameters-prefix \, :formal-parameter) formal-parameters-prefix-more)
       
       (production :formal-parameter ($identifier (:type-declaration normal)) formal-parameter-identifier)
       
       
       (%section "Classes")
       
       (production (:member-definition :omega_3) (:accessibility (:local-definition :omega_3)) member-definition-local-definition)
       
       
       (%subsection "Accessibility Specifications")
       (production :accessibility (private) accessibility-private)
       (production :accessibility () accessibility-package)
       (production :accessibility (public :versions-and-renames) accessibility-public)
       
       (production :versions-and-renames () versions-and-renames-default)
       (production :versions-and-renames (< :version-ranges-and-aliases >) versions-and-renames-version-ranges-and-aliases)
       
       (production :version-ranges-and-aliases () version-ranges-and-aliases-none)
       (production :version-ranges-and-aliases (:version-ranges-and-aliases-prefix) version-ranges-and-aliases-some)
       
       (production :version-ranges-and-aliases-prefix (:version-range-and-alias) version-ranges-and-aliases-prefix-one)
       (production :version-ranges-and-aliases-prefix (:version-ranges-and-aliases-prefix \, :version-range-and-alias) version-ranges-and-aliases-prefix-more)
       
       (production :version-range-and-alias (:version-range) version-range-and-alias-version-range)
       (production :version-range-and-alias (:version-range \: $identifier) version-range-and-alias-version-range-and-alias)
       
       (production :version-range (:version) version-range-singleton)
       (production :version-range (:optional-version \.\. :optional-version) version-range-range)
       
       
       (%subsection "Versions")
       (production :optional-version (:version) optional-version-version)
       (production :optional-version () optional-version-none)
       
       (production :version ($number) version-number)
       (production :version ($string) version-string)
       
       
       
       
       (%section "Programs")
       
       (production :program (:top-statements) program)
       
       (production :top-statements ((:top-statement abbrev)) top-statements-one)
       (production :top-statements (:top-statements-prefix (:top-statement abbrev-non-empty)) top-statements-more)
       
       (production :top-statements-prefix ((:top-statement full)) top-statements-prefix-one)
       (production :top-statements-prefix (:top-statements-prefix (:top-statement full)) top-statements-prefix-more)
       
       (production (:top-statement :omega_3) ((:statement :omega_3)) top-statement-statement)
       (production (:top-statement :omega_3) ((:member-definition :omega_3)) top-statement-member-definition)
       )))
  
  (defparameter *jg* (world-grammar *jw* 'code-grammar))
  (length (grammar-states *jg*)))


#|
(let ((*visible-modes* nil))
  (depict-rtf-to-local-file
   "JS20.rtf"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *jw*))))

(let ((*visible-modes* nil))
  (depict-html-to-local-file
   "JS20.html"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *jw*))
   "JavaScript 2.0 Grammar"))

(with-local-output (s "JS20.txt") (print-grammar *jg* s))
|#
