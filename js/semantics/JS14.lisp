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
;;; Sample JavaScript 1.x grammar
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
       
       (%subsection "Primary Expressions")
       (production (:primary-expression :alpha) (:simple-expression) primary-expression-simple-expression)
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
       
       (production :parenthesized-expression (\( (:expression normal allow-in) \)) parenthesized-expression-expression)
       
       
       (%subsection "Function Expressions")
       (production :function-expression (:anonymous-function) function-expression-anonymous-function)
       (production :function-expression (:named-function) function-expression-named-function)
       
       
       (%subsection "Object Literals")
       (production :object-literal (\{ \}) object-literal-empty)
       (production :object-literal (\{ :field-list \}) object-literal-list)
       
       (production :field-list (:literal-field) field-list-one)
       (production :field-list (:field-list \, :literal-field) field-list-more)
       
       (production :literal-field ($identifier \: (:assignment-expression normal allow-in)) literal-field-assignment-expression)
       
       
       (%subsection "Array Literals")
       (production :array-literal ([ ]) array-literal-empty)
       (production :array-literal ([ :element-list ]) array-literal-list)
       
       (production :element-list (:literal-element) element-list-one)
       (production :element-list (:element-list \, :literal-element) element-list-more)
       
       (production :literal-element ((:assignment-expression normal allow-in)) literal-element-assignment-expression)
       
       
       (%subsection "Left-Side Expressions")
       (grammar-argument :chi allow-calls no-calls)
       
       (production (:member-expression :alpha no-calls) ((:primary-expression :alpha)) member-expression-primary-expression)
       (production (:member-expression :alpha allow-calls) ((:member-expression :alpha no-calls) :arguments) call-expression-call-member-expression)
       (production (:member-expression :alpha allow-calls) ((:member-expression :alpha allow-calls) :arguments) call-expression-call-call-expression)
       (production (:member-expression :alpha :chi) ((:member-expression :alpha :chi) [ (:expression normal allow-in) ]) member-expression-array)
       (production (:member-expression :alpha :chi) ((:member-expression :alpha :chi) \. $identifier) member-expression-property)
       (production (:member-expression :alpha no-calls) (new (:member-expression normal no-calls) :arguments) member-expression-new)
       
       (production (:new-expression :alpha) ((:member-expression :alpha no-calls)) new-expression-member-expression)
       (production (:new-expression :alpha) (new (:new-expression normal)) new-expression-new)
       
       (production :arguments (\( \)) arguments-empty)
       (production :arguments (\( :argument-list \)) arguments-list)
       
       (production :argument-list ((:assignment-expression normal allow-in)) argument-list-one)
       (production :argument-list (:argument-list \, (:assignment-expression normal allow-in)) argument-list-more)
       
       (production (:left-side-expression :alpha) ((:new-expression :alpha)) left-side-expression-new-expression)
       (production (:left-side-expression :alpha) ((:member-expression :alpha allow-calls)) left-side-expression-call-expression)
       
       
       (%subsection "Postfix Expressions")
       (production (:postfix-expression :alpha) ((:left-side-expression :alpha)) postfix-expression-left-side-expression)
       (production (:postfix-expression :alpha) ((:left-side-expression :alpha) ++) postfix-expression-increment)
       (production (:postfix-expression :alpha) ((:left-side-expression :alpha) --) postfix-expression-decrement)
       
       
       (%subsection "Unary Operators")
       (production (:unary-expression :alpha) ((:postfix-expression :alpha)) unary-expression-postfix)
       (production (:unary-expression :alpha) (delete (:left-side-expression normal)) unary-expression-delete)
       (production (:unary-expression :alpha) (void (:unary-expression normal)) unary-expression-void)
       (production (:unary-expression :alpha) (typeof (:unary-expression normal)) unary-expression-typeof)
       (production (:unary-expression :alpha) (++ (:left-side-expression normal)) unary-expression-increment)
       (production (:unary-expression :alpha) (-- (:left-side-expression normal)) unary-expression-decrement)
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
       (production (:relational-expression :alpha :beta) ((:shift-expression :alpha)) relational-expression-shift)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) < (:shift-expression normal)) relational-expression-less)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) > (:shift-expression normal)) relational-expression-greater)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) <= (:shift-expression normal)) relational-expression-less-or-equal)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) >= (:shift-expression normal)) relational-expression-greater-or-equal)
       (production (:relational-expression :alpha :beta) ((:relational-expression :alpha :beta) instanceof (:shift-expression normal)) relational-expression-instanceof)
       (production (:relational-expression :alpha allow-in) ((:relational-expression :alpha allow-in) in (:shift-expression normal)) relational-expression-in)
       
       
       (%subsection "Equality Operators")
       (production (:equality-expression :alpha :beta) ((:relational-expression :alpha :beta)) equality-expression-relational)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) == (:relational-expression normal :beta)) equality-expression-equal)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) != (:relational-expression normal :beta)) equality-expression-not-equal)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) === (:relational-expression normal :beta)) equality-expression-strict-equal)
       (production (:equality-expression :alpha :beta) ((:equality-expression :alpha :beta) !== (:relational-expression normal :beta)) equality-expression-strict-not-equal)
       
       
       (%subsection "Binary Bitwise Operators")
       (production (:bitwise-and-expression :alpha :beta) ((:equality-expression :alpha :beta)) bitwise-and-expression-equality)
       (production (:bitwise-and-expression :alpha :beta) ((:bitwise-and-expression :alpha :beta) & (:equality-expression normal :beta)) bitwise-and-expression-and)
       
       (production (:bitwise-xor-expression :alpha :beta) ((:bitwise-and-expression :alpha :beta)) bitwise-xor-expression-bitwise-and)
       (production (:bitwise-xor-expression :alpha :beta) ((:bitwise-xor-expression :alpha :beta) ^ (:bitwise-and-expression normal :beta)) bitwise-xor-expression-xor)
       
       (production (:bitwise-or-expression :alpha :beta) ((:bitwise-xor-expression :alpha :beta)) bitwise-or-expression-bitwise-xor)
       (production (:bitwise-or-expression :alpha :beta) ((:bitwise-or-expression :alpha :beta) \| (:bitwise-xor-expression normal :beta)) bitwise-or-expression-or)
       
       
       (%subsection "Binary Logical Operators")
       (production (:logical-and-expression :alpha :beta) ((:bitwise-or-expression :alpha :beta)) logical-and-expression-bitwise-or)
       (production (:logical-and-expression :alpha :beta) ((:logical-and-expression :alpha :beta) && (:bitwise-or-expression normal :beta)) logical-and-expression-and)
       
       (production (:logical-or-expression :alpha :beta) ((:logical-and-expression :alpha :beta)) logical-or-expression-logical-and)
       (production (:logical-or-expression :alpha :beta) ((:logical-or-expression :alpha :beta) \|\| (:logical-and-expression normal :beta)) logical-or-expression-or)
       
       
       (%subsection "Conditional Operator")
       (production (:conditional-expression :alpha :beta) ((:logical-or-expression :alpha :beta)) conditional-expression-logical-or)
       (production (:conditional-expression :alpha :beta) ((:logical-or-expression :alpha :beta) ? (:assignment-expression normal :beta) \: (:assignment-expression normal :beta)) conditional-expression-conditional)
       
       
       (%subsection "Assignment Operators")
       (production (:assignment-expression :alpha :beta) ((:conditional-expression :alpha :beta)) assignment-expression-conditional)
       (production (:assignment-expression :alpha :beta) ((:left-side-expression :alpha) = (:assignment-expression normal :beta)) assignment-expression-assignment)
       (production (:assignment-expression :alpha :beta) ((:left-side-expression :alpha) :compound-assignment (:assignment-expression normal :beta)) assignment-expression-compound)
       
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
       (production (:expression :alpha :beta) ((:assignment-expression :alpha :beta)) expression-assignment)
       (production (:expression :alpha :beta) ((:expression :alpha :beta) \, (:assignment-expression normal :beta)) expression-comma)
       
       (production :optional-expression ((:expression normal allow-in)) optional-expression-expression)
       (production :optional-expression () optional-expression-empty)
              
       
       (%section "Statements")
       
       (grammar-argument :omega
                         no-short-if ;optional semicolon, but statement must not end with an if without an else
                         full) ;semicolon required at the end
       
       (production (:statement :omega) (:empty-statement) statement-empty-statement)
       (production (:statement :omega) (:expression-statement :optional-semicolon) statement-expression-statement)
       (production (:statement :omega) (:variable-definition :optional-semicolon) statement-variable-definition)
       (production (:statement :omega) (:block) statement-block)
       (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement)
       (production (:statement :omega) ((:if-statement :omega)) statement-if-statement)
       (production (:statement :omega) (:switch-statement) statement-switch-statement)
       (production (:statement :omega) (:do-statement :optional-semicolon) statement-do-statement)
       (production (:statement :omega) ((:while-statement :omega)) statement-while-statement)
       (production (:statement :omega) ((:for-statement :omega)) statement-for-statement)
       (production (:statement :omega) ((:with-statement :omega)) statement-with-statement)
       (production (:statement :omega) (:continue-statement :optional-semicolon) statement-continue-statement)
       (production (:statement :omega) (:break-statement :optional-semicolon) statement-break-statement)
       (production (:statement :omega) (:return-statement :optional-semicolon) statement-return-statement)
       (production (:statement :omega) (:throw-statement :optional-semicolon) statement-throw-statement)
       (production (:statement :omega) (:try-statement) statement-try-statement)
       
       (production :optional-semicolon (\;) optional-semicolon-semicolon)
       
       
       (%subsection "Empty Statement")
       (production :empty-statement (\;) empty-statement-semicolon)
       
       
       (%subsection "Expression Statement")
       (production :expression-statement ((:expression initial allow-in)) expression-statement-expression)
       
       
       (%subsection "Variable Definition")
       (production :variable-definition (var (:variable-declaration-list allow-in)) variable-definition-declaration)
       
       (production (:variable-declaration-list :beta) ((:variable-declaration :beta)) variable-declaration-list-one)
       (production (:variable-declaration-list :beta) ((:variable-declaration-list :beta) \, (:variable-declaration :beta)) variable-declaration-list-more)
       
       (production (:variable-declaration :beta) ($identifier (:variable-initializer :beta)) variable-declaration-initializer)
       
       (production (:variable-initializer :beta) () variable-initializer-empty)
       (production (:variable-initializer :beta) (= (:assignment-expression normal :beta)) variable-initializer-assignment-expression)
       
       
       (%subsection "Block")
       (production :block ({ :block-statements }) block-block-statements)
       
       (production :block-statements () block-statements-one)
       (production :block-statements (:block-statements-prefix) block-statements-more)
       
       (production :block-statements-prefix ((:statement full)) block-statements-prefix-one)
       (production :block-statements-prefix (:block-statements-prefix (:statement full)) block-statements-prefix-more)
       
       
       (%subsection "Labeled Statements")
       (production (:labeled-statement :omega) ($identifier \: (:statement :omega)) labeled-statement-label)
       
       
       (%subsection "If Statement")
       (production (:if-statement full) (if :parenthesized-expression (:statement full)) if-statement-if-then-full)
       (production (:if-statement :omega) (if :parenthesized-expression (:statement no-short-if)
                                              else (:statement :omega)) if-statement-if-then-else)
       
       
       (%subsection "Switch Statement")
       (production :switch-statement (switch :parenthesized-expression { }) switch-statement-empty)
       (production :switch-statement (switch :parenthesized-expression { :case-groups :last-case-group }) switch-statement-cases)
       
       (production :case-groups () case-groups-empty)
       (production :case-groups (:case-groups :case-group) case-groups-more)
       
       (production :case-group (:case-guards :block-statements-prefix) case-group-block-statements-prefix)
       
       (production :last-case-group (:case-guards :block-statements) last-case-group-block-statements)
       
       (production :case-guards (:case-guard) case-guards-one)
       (production :case-guards (:case-guards :case-guard) case-guards-more)
       
       (production :case-guard (case (:expression normal allow-in) \:) case-guard-case)
       (production :case-guard (default \:) case-guard-default)
       
       
       (%subsection "Do-While Statement")
       (production :do-statement (do (:statement full) while :parenthesized-expression) do-statement-do-while)
       
       
       (%subsection "While Statement")
       (production (:while-statement :omega) (while :parenthesized-expression (:statement :omega)) while-statement-while)
       
       
       (%subsection "For Statements")
       (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                                (:statement :omega)) for-statement-c-style)
       (production (:for-statement :omega) (for \( :for-in-binding in (:expression normal allow-in) \) (:statement :omega)) for-statement-in)
       
       (production :for-initializer () for-initializer-empty)
       (production :for-initializer ((:expression normal no-in)) for-initializer-expression)
       (production :for-initializer (var (:variable-declaration-list no-in)) for-initializer-variable-declaration)
       
       (production :for-in-binding ((:left-side-expression normal)) for-in-binding-expression)
       (production :for-in-binding (var (:variable-declaration no-in)) for-in-binding-variable-declaration)
       
       
       (%subsection "With Statement")
       (production (:with-statement :omega) (with :parenthesized-expression (:statement :omega)) with-statement-with)
       
       
       (%subsection "Continue and Break Statements")
       (production :continue-statement (continue :optional-label) continue-statement-optional-label)
       
       (production :break-statement (break :optional-label) break-statement-optional-label)
       
       (production :optional-label () optional-label-default)
       (production :optional-label ($identifier) optional-label-identifier)
       
       
       (%subsection "Return Statement")
       (production :return-statement (return :optional-expression) return-statement-optional-expression)
       
       
       (%subsection "Throw Statement")
       (production :throw-statement (throw (:expression normal allow-in)) throw-statement-throw)
       
       
       (%subsection "Try Statement")
       (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses)
       (production :try-statement (try :block :finally-clause) try-statement-finally-clause)
       (production :try-statement (try :block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
       
       (production :catch-clauses (:catch-clause) catch-clauses-one)
       (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
       
       (production :catch-clause (catch \( $identifier \) :block) catch-clause-block)
       
       (production :finally-clause (finally :block) finally-clause-block)
       
       
       (%subsection "Function Definition")
       (production :function-definition (:named-function) function-definition-named-function)
       
       (production :anonymous-function (function :formal-parameters-and-body) anonymous-function-formal-parameters-and-body)
       
       (production :named-function (function $identifier :formal-parameters-and-body) named-function-formal-parameters-and-body)
       
       (production :formal-parameters-and-body (\( :formal-parameters \) { :top-statements }) formal-parameters-and-body)
       
       (production :formal-parameters () formal-parameters-none)
       (production :formal-parameters (:formal-parameters-prefix) formal-parameters-some)
       
       (production :formal-parameters-prefix (:formal-parameter) formal-parameters-prefix-one)
       (production :formal-parameters-prefix (:formal-parameters-prefix \, :formal-parameter) formal-parameters-prefix-more)
       
       (production :formal-parameter ($identifier) formal-parameter-identifier)
       
       
       (%section "Programs")
       
       (production :program (:top-statements) program)
       
       (production :top-statements () top-statements-one)
       (production :top-statements (:top-statements-prefix) top-statements-more)
       
       (production :top-statements-prefix (:top-statement) top-statements-prefix-one)
       (production :top-statements-prefix (:top-statements-prefix :top-statement) top-statements-prefix-more)
       
       (production :top-statement ((:statement full)) top-statement-statement)
       (production :top-statement (:function-definition) top-statement-function-definition)
       )))
  
  (defparameter *jg* (world-grammar *jw* 'code-grammar))
  (length (grammar-states *jg*)))


#|
(let ((*visible-modes* nil))
  (depict-rtf-to-local-file
   "JS14.rtf"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *jw*))))

(let ((*visible-modes* nil))
  (depict-html-to-local-file
   "JS14.html"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *jw*))
   "JavaScript 1.4 Grammar"))

(with-local-output (s "JS14.txt") (print-grammar *jg* s))
|#
