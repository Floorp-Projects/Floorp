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
       
       (%subsection "Primary Expressions")
       (production :primary-expression (this) primary-expression-this)
       (production :primary-expression (null) primary-expression-null)
       (production :primary-expression (true) primary-expression-true)
       (production :primary-expression (false) primary-expression-false)
       (production :primary-expression ($number) primary-expression-number)
       (production :primary-expression ($string) primary-expression-string)
       (production :primary-expression ($identifier) primary-expression-identifier)
       (production :primary-expression ($regular-expression) primary-expression-regular-expression)
       (production :primary-expression (\( :expression \)) primary-expression-parentheses)
       
       
       (%subsection "Left-Side Expressions")
       (grammar-argument :chi allow-calls no-calls)
       (grammar-argument :alpha allow-in no-in)
       
       (production (:member-expression no-calls) (:primary-expression) member-expression-primary-expression)
       (production (:member-expression allow-calls) ((:member-expression no-calls) :arguments) call-expression-call-member-expression)
       (production (:member-expression allow-calls) ((:member-expression allow-calls) :arguments) call-expression-call-call-expression)
       (production (:member-expression :chi) ((:member-expression :chi) \[ :expression \]) member-expression-array)
       (production (:member-expression :chi) ((:member-expression :chi) \. $identifier) member-expression-property)
       (production (:member-expression no-calls) (new (:member-expression no-calls) :arguments) member-expression-new)
       
       (production :new-expression ((:member-expression no-calls)) new-expression-member-expression)
       (production :new-expression (new :new-expression) new-expression-new)
       
       (production :arguments (\( \)) arguments-empty)
       (production :arguments (\( :argument-list \)) arguments-list)
       
       (production :argument-list ((:assignment-expression allow-in)) argument-list-one)
       (production :argument-list (:argument-list \, (:assignment-expression allow-in)) argument-list-more)
       
       (production :left-side-expression (:new-expression) left-side-expression-new-expression)
       (production :left-side-expression ((:member-expression allow-calls)) left-side-expression-call-expression)
       
       
       (%subsection "Postfix Expressions")
       (production :postfix-expression (:left-side-expression) postfix-expression-left-side-expression)
       (production :postfix-expression (:left-side-expression ++) postfix-expression-increment)
       (production :postfix-expression (:left-side-expression --) postfix-expression-decrement)
       
       
       (%subsection "Unary Operators")
       (production :unary-expression (:postfix-expression) unary-expression-postfix)
       (production :unary-expression (delete :left-side-expression) unary-expression-delete)
       (production :unary-expression (void :unary-expression) unary-expression-void)
       (production :unary-expression (typeof :unary-expression) unary-expression-typeof)
       (production :unary-expression (++ :left-side-expression) unary-expression-increment)
       (production :unary-expression (-- :left-side-expression) unary-expression-decrement)
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
       (production (:relational-expression :alpha) (:shift-expression) relational-expression-shift)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) < :shift-expression) relational-expression-less)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) > :shift-expression) relational-expression-greater)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) <= :shift-expression) relational-expression-less-or-equal)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) >= :shift-expression) relational-expression-greater-or-equal)
       (production (:relational-expression :alpha) ((:relational-expression :alpha) instanceof :shift-expression) relational-expression-instanceof)
       (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression) relational-expression-in)
       
       
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
       (production (:assignment-expression :alpha) (:left-side-expression = (:assignment-expression :alpha)) assignment-expression-assignment)
       (production (:assignment-expression :alpha) (:left-side-expression :compound-assignment (:assignment-expression :alpha)) assignment-expression-compound)
       
       (production :compound-assignment (*=) compound-assignment-multiply)
       (production :compound-assignment (/=) compound-assignment-divide)
       (production :compound-assignment (%=) compound-assignment-remainder)
       (production :compound-assignment (+=) compound-assignment-add)
       (production :compound-assignment (-=) compound-assignment-subtract)
       
       
       (%subsection "Expressions")
       (production (:comma-expression :alpha) ((:assignment-expression :alpha)) comma-expression-assignment)
       
       (production :expression ((:comma-expression allow-in)) expression-comma-expression)
       
       (production :optional-expression (:expression) optional-expression-expression)
       (production :optional-expression () optional-expression-empty)
       
       
       (%section "Statements")
       
       (grammar-argument :omega
                         abbrev ;optional semicolon
                         abbrev-non-empty ;optional semicolon as long as statement isn't empty
                         abbrev-no-short-if ;optional semicolon, but statement must not end with an if without an else
                         full) ;semicolon required at the end
       
       (production (:statement :omega) (:blocklike-statement) statement-blocklike-statement)
       (production (:statement :omega) (:unterminated-statement \;) statement-unterminated-statement)
       (production (:statement :omega) ((:nonuniform-statement :omega)) statement-nonuniform-statement)
       (production (:statement :omega) ((:if-statement :omega)) statement-if-statement)
       (production (:statement :omega) ((:while-statement :omega)) statement-while-statement)
       (production (:statement :omega) ((:for-statement :omega)) statement-for-statement)
       (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement)
       
       ;Statements that differ depending on omega
       (production (:nonuniform-statement :omega) (:empty-statement \;) nonuniform-statement-empty-statement)
       (production (:nonuniform-statement abbrev) (:empty-statement) nonuniform-statement-empty-statement-abbrev)
       (production (:nonuniform-statement abbrev) (:unterminated-statement) nonuniform-statement-unterminated-statement-abbrev)
       (production (:nonuniform-statement abbrev-non-empty) (:unterminated-statement) nonuniform-statement-unterminated-statement-abbrev-non-empty)
       (production (:nonuniform-statement abbrev-no-short-if) (:unterminated-statement) nonuniform-statement-unterminated-statement-abbrev-no-short-if)
       (production (:nonuniform-statement abbrev-no-short-if) (:empty-statement) nonuniform-statement-empty-statement-abbrev-no-short-if)
       
       ;Statements that always end with a '}'
       (production :blocklike-statement (:block) blocklike-statement-block)
       (production :blocklike-statement (:switch-statement) blocklike-statement-switch-statement)
       (production :blocklike-statement (:try-statement) blocklike-statement-try-statement)
       
       ;Statements that must be followed by a semicolon unless followed by a '}', 'else', or 'while' in a do-while
       (production :unterminated-statement (:variable-statement) unterminated-statement-variable-statement)
       (production :unterminated-statement (:expression-statement) unterminated-statement-expression-statement)
       (production :unterminated-statement (:do-statement) unterminated-statement-do-statement)
       (production :unterminated-statement (:continue-statement) unterminated-statement-continue-statement)
       (production :unterminated-statement (:break-statement) unterminated-statement-break-statement)
       (production :unterminated-statement (:return-statement) unterminated-statement-return-statement)
       (production :unterminated-statement (:throw-statement) unterminated-statement-throw-statement)
       
       
       (%subsection "Block")
       (production :block ({ :block-statements }) block-block-statements)
       
       (production :block-statements ((:statement abbrev)) block-statements-one)
       (production :block-statements (:block-statements-prefix (:statement abbrev-non-empty)) block-statements-more)
       
       (production :block-statements-prefix ((:statement full)) block-statements-prefix-one)
       (production :block-statements-prefix (:block-statements-prefix (:statement full)) block-statements-prefix-more)
       
       
       (%subsection "Variable Statement")
       (production :variable-statement (var (:variable-declaration-list allow-in)) variable-statement-declaration)
       
       (production (:variable-declaration-list :alpha) ((:variable-declaration :alpha)) variable-declaration-list-one)
       (production (:variable-declaration-list :alpha) ((:variable-declaration-list :alpha) \, (:variable-declaration :alpha)) variable-declaration-list-more)
       
       (production (:variable-declaration :alpha) ($identifier) variable-declaration-identifier)
       (production (:variable-declaration :alpha) ($identifier = (:assignment-expression :alpha)) variable-declaration-initializer)
       
       
       (%subsection "Empty Statement")
       (production :empty-statement () empty-statement-empty)
       
       
       (%subsection "Expression Statement")
       (production :expression-statement (:expression) expression-statement-expression)
       
       
       (%subsection "If Statement")
       (production (:if-statement abbrev) (if \( :expression \) (:statement abbrev)) if-statement-if-then-abbrev)
       (production (:if-statement abbrev-non-empty) (if \( :expression \) (:statement abbrev-non-empty)) if-statement-if-then-abbrev-non-empty)
       (production (:if-statement full) (if \( :expression \) (:statement full)) if-statement-if-then-full)
       (production (:if-statement :omega) (if \( :expression \) (:statement abbrev-no-short-if)
                                              else (:statement :omega)) if-statement-if-then-else)
       
       
       (%subsection "Do-While Statement")
       (production :do-statement (do (:statement abbrev-non-empty) while \( :expression \)) do-statement-do-while)
       
       
       (%subsection "While Statement")
       (production (:while-statement :omega) (while \( :expression \) (:statement :omega)) while-statement-while)
       
       
       (%subsection "For Statements")
       (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                                (:statement :omega)) for-statement-c-style)
       (production (:for-statement :omega) (for \( :for-in-binding in :expression \) (:statement :omega)) for-statement-in)
       
       (production :for-initializer () for-initializer-empty)
       (production :for-initializer ((:comma-expression no-in)) for-initializer-expression)
       (production :for-initializer (var (:variable-declaration-list no-in)) for-initializer-variable-declaration)
       
       (production :for-in-binding (:left-side-expression) for-in-binding-expression)
       (production :for-in-binding (var (:variable-declaration no-in)) for-in-binding-variable-declaration)
       
       
       (%subsection "Continue and Break Statements")
       (production :continue-statement (continue :optional-label) continue-statement-optional-label)
       
       (production :break-statement (break :optional-label) break-statement-optional-label)
       
       (production :optional-label () optional-label-default)
       (production :optional-label ($identifier) optional-label-identifier)
       
       
       (%subsection "Labeled Statements")
       (production (:labeled-statement :omega) ($identifier \: (:statement :omega)) labeled-statement-label)
       
       
       (%subsection "Return Statement")
       (production :return-statement (return :optional-expression) return-statement-optional-expression)
       
       
       (%subsection "Switch Statement")
       (production :switch-statement (switch \( :expression \) { }) switch-statement-empty)
       (production :switch-statement (switch \( :expression \) { :case-groups :last-case-group }) switch-statement-cases)
       
       (production :case-groups () case-groups-empty)
       (production :case-groups (:case-groups :case-group) case-groups-more)
       
       (production :case-group (:case-guards :block-statements-prefix) case-group-block-statements-prefix)
       
       (production :last-case-group (:case-guards :block-statements) last-case-group-block-statements)
       
       (production :case-guards (:case-guard) case-guards-one)
       (production :case-guards (:case-guards :case-guard) case-guards-more)
       
       (production :case-guard (case :expression \:) case-guard-case)
       (production :case-guard (default \:) case-guard-default)
       
       
       (%subsection "Throw Statement")
       (production :throw-statement (throw :expression) throw-statement-throw)
       
       
       (%subsection "Try Statement")
       (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses)
       (production :try-statement (try :block :finally-clause) try-statement-finally-clause)
       (production :try-statement (try :block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
       
       (production :catch-clauses (:catch-clause) catch-clauses-one)
       (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
       
       (production :catch-clause (catch \( $identifier \) :block) catch-clause-block)
       
       (production :finally-clause (finally :block) finally-clause-block)
       
       
       (%section "Functions")
       
       (production :function-declaration (function $identifier \( :formal-parameters \) { :function-statements }) function-declaration-statements)
       
       (production :formal-parameters () formal-parameters-none)
       (production :formal-parameters (:formal-parameters-prefix) formal-parameters-some)
       
       (production :formal-parameters-prefix ($identifier) formal-parameters-prefix-one)
       (production :formal-parameters-prefix (:formal-parameters-prefix \, $identifier) formal-parameters-prefix-more)
       
       (production :function-statements ((:function-statement abbrev)) function-statements-one)
       (production :function-statements (:function-statements-prefix (:function-statement abbrev-non-empty)) function-statements-more)
       
       (production :function-statements-prefix ((:function-statement full)) function-statements-prefix-one)
       (production :function-statements-prefix (:function-statements-prefix (:function-statement full)) function-statements-prefix-more)

       (production (:function-statement :omega) ((:statement :omega)) function-statement-statement)
       (production (:function-statement :omega) (:function-declaration) function-statement-function-declaration)
       
       
       (%section "Programs")
       
       (production :program (:function-statements) program)
       )))
  
  (defparameter *jg* (world-grammar *jw* 'code-grammar)))


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
   "JavaScript 2.0 Grammar"))

(with-local-output (s "JS14.txt") (print-grammar *jg* s))
|#
