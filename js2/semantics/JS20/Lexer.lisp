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
;;; Copyright (C) 1998 Netscape Communications Corporation.  All Rights
;;; Reserved.

;;;
;;; JavaScript 2.0 lexer
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;


(progn
  (defparameter *lw*
    (generate-world
     "L"
     '((lexer code-lexer
              :lalr-1
              :$next-token
              ((:unicode-character (% every (:text "Any Unicode character")) () t)
               (:unicode-initial-alphabetic
                (% initial-alpha (:text "Any Unicode initial alphabetic character (includes ASCII "
                                        (:character-literal #\A) :nbhy (:character-literal #\Z) " and "
                                        (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:unicode-alphanumeric
                (% alphanumeric (:text "Any Unicode alphabetic or decimal digit character (includes ASCII "
                                       (:character-literal #\0) :nbhy (:character-literal #\9) ", "
                                       (:character-literal #\A) :nbhy (:character-literal #\Z) ", and "
                                       (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:white-space-character (++ (#?0009 #?000B #?000C #\space #?00A0)
                                           (#?2000 #?2001 #?2002 #?2003 #?2004 #?2005 #?2006 #?2007)
                                           (#?2008 #?2009 #?200A #?200B)
                                           (#?3000)) ())
               (:line-terminator (#?000A #?000D #?2028 #?2029) ())
               (:non-terminator (- :unicode-character :line-terminator)
                                (($default-action $default-action)))
               (:non-slash (- :unicode-character (#\/)) ())
               (:non-asterisk-or-slash (- :unicode-character (#\* #\/)) ())
               (:ordinary-initial-identifier-character (+ :unicode-initial-alphabetic (#\$ #\_))
                                                       (($default-action $default-action)))
               (:ordinary-continuing-identifier-character (+ :unicode-alphanumeric (#\$ #\_))
                                                          (($default-action $default-action)))
               (:a-s-c-i-i-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                 (($default-action $default-action)
                                  (decimal-value $digit-value)))
               (:non-zero-digit (#\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                ((decimal-value $digit-value)))
               (:octal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7)
                             (($default-action $default-action)
                              (octal-value $digit-value)))
               (:zero-to-three (#\0 #\1 #\2 #\3)
                               ((octal-value $digit-value)))
               (:four-to-seven (#\4 #\5 #\6 #\7)
                               ((octal-value $digit-value)))
               (:hex-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\A #\B #\C #\D #\E #\F #\a #\b #\c #\d #\e #\f)
                           ((hex-value $digit-value)))
               (:letter-e (#\E #\e) (($default-action $default-action)))
               (:letter-x (#\X #\x) (($default-action $default-action)))
               ((:literal-string-char single) (- :unicode-character (+ (#\' #\\) :line-terminator))
                (($default-action $default-action)))
               ((:literal-string-char double) (- :unicode-character (+ (#\" #\\) :line-terminator))
                (($default-action $default-action)))
               (:identity-escape (- :non-terminator :unicode-alphanumeric)
                                 (($default-action $default-action)))
               (:ordinary-reg-exp-first-char (- :non-terminator (#\\ #\/ #\*))
                                             (($default-action $default-action)))
               ((:ordinary-reg-exp-char slash) (- :non-terminator (#\\ #\/))
                (($default-action $default-action)))
               ((:ordinary-reg-exp-char guillemet) (- :non-terminator (#\\ #?00BB))
                (($default-action $default-action))))
              (($default-action character nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       (rule :$next-token
             ((token token) (reg-exp-may-follow boolean))
         (production :$next-token ($re (:next-token re)) $next-token-re
           (token (token :next-token))
           (reg-exp-may-follow (reg-exp-may-follow :next-token)))
         (production :$next-token ($non-re (:next-token div)) $next-token-non-re
           (token (token :next-token))
           (reg-exp-may-follow (reg-exp-may-follow :next-token))))
       
       (%text nil "The start symbols are " (:grammar-symbol (:next-token re)) " and " (:grammar-symbol (:next-token div))
              " depending on whether a " (:character-literal #\/) " should be interpreted as a regular expression or division.")
       
       (%section "Unicode Character Classes")
       (%charclass :unicode-character)
       (%charclass :unicode-initial-alphabetic)
       (%charclass :unicode-alphanumeric)
       (%charclass :white-space-character)
       (%charclass :line-terminator)
       (%charclass :a-s-c-i-i-digit)
       (%print-actions)
       
       (%section "Comments")
       (production :line-comment (#\/ #\/ :line-comment-characters) line-comment)
       
       (production :line-comment-characters () line-comment-characters-empty)
       (production :line-comment-characters (:line-comment-characters :non-terminator) line-comment-characters-chars)
       
       (%charclass :non-terminator)
       
       (production :block-comment (#\/ #\* :block-comment-characters #\* #\/) block-comment)
       
       (production :block-comment-characters () block-comment-characters-empty)
       (production :block-comment-characters (:block-comment-characters :non-slash) block-comment-characters-chars)
       (production :block-comment-characters (:pre-slash-characters #\/) block-comment-characters-slash)
       
       (production :pre-slash-characters () pre-slash-characters-empty)
       (production :pre-slash-characters (:block-comment-characters :non-asterisk-or-slash) pre-slash-characters-chars)
       (production :pre-slash-characters (:pre-slash-characters #\/) pre-slash-characters-slash)
       
       (%charclass :non-slash)
       (%charclass :non-asterisk-or-slash)
       (%print-actions)
       
       (%section "White space")
       
       (production :white-space () white-space-empty)
       (production :white-space (:white-space :white-space-character) white-space-character)
       (production :white-space (:white-space :line-terminator) white-space-line-terminator)
       (production :white-space (:white-space :line-comment :line-terminator) white-space-line-comment)
       (production :white-space (:white-space :block-comment) white-space-block-comment)
       
       (%section "Tokens")
       
       (grammar-argument :tau re div)
       
       (rule (:next-token :tau)
             ((token token) (reg-exp-may-follow boolean))
         (production (:next-token :tau) (:white-space (:token :tau)) next-token
           (token (token :token))
           (reg-exp-may-follow (reg-exp-may-follow :token))))
       
       (rule (:token :tau)
             ((token token) (reg-exp-may-follow boolean))
         (production (:token :tau) (:identifier-or-reserved-word) token-identifier-or-reserved-word
           (token (token :identifier-or-reserved-word))
           (reg-exp-may-follow (reg-exp-may-follow :identifier-or-reserved-word)))
         (production (:token :tau) (:punctuator) token-punctuator
           (token (token :punctuator))
           (reg-exp-may-follow (reg-exp-may-follow :punctuator)))
         (production (:token div) (:division-punctuator) token-division-punctuator
           (token (oneof punctuator (punctuator :division-punctuator)))
           (reg-exp-may-follow true))
         (production (:token :tau) (:numeric-literal) token-numeric-literal
           (token (oneof number (double-value :numeric-literal)))
           (reg-exp-may-follow false))
         (production (:token :tau) (:quantity-literal) token-quantity-literal
           (token (oneof quantity (quantity-value :quantity-literal)))
           (reg-exp-may-follow false))
         (production (:token :tau) (:string-literal) token-string-literal
           (token (oneof string (string-value :string-literal)))
           (reg-exp-may-follow false))
         (production (:token re) ((:reg-exp-literal slash)) token-reg-exp-literal-slash
           (token (oneof regular-expression (r-e-value :reg-exp-literal)))
           (reg-exp-may-follow false))
         (production (:token :tau) ((:reg-exp-literal guillemet)) token-reg-exp-literal-guillemet
           (token (oneof regular-expression (r-e-value :reg-exp-literal)))
           (reg-exp-may-follow false))
         (production (:token :tau) (:end-of-input) token-end
           (token (oneof end))
           (reg-exp-may-follow true)))
       
       (production :end-of-input ($end) end-of-input-end)
       (production :end-of-input (:line-comment $end) end-of-input-line-comment)
       
       (deftype reg-exp (tuple (re-body string)
                          (re-flags string)))
       
       (deftype quantity (tuple (amount double)
                           (unit string)))
       
       (deftype token (oneof (identifier string)
                             (keyword string)
                             (punctuator string)
                             (number double)
                             (quantity quantity)
                             (string string)
                             (regular-expression reg-exp)
                             end))
       (%print-actions)
       
       (%section "Keywords and identifiers")
       
       (rule :identifier-name
             ((name string) (contains-escapes boolean))
         (production :identifier-name (:initial-identifier-character) identifier-name-initial
           (name (vector (character-value :initial-identifier-character)))
           (contains-escapes (contains-escapes :initial-identifier-character)))
         (production :identifier-name (:identifier-name :continuing-identifier-character) identifier-name-continuing
           (name (append (name :identifier-name) (vector (character-value :continuing-identifier-character))))
           (contains-escapes (or (contains-escapes :identifier-name)
                                 (contains-escapes :continuing-identifier-character)))))
       
       (rule :initial-identifier-character
             ((character-value character) (contains-escapes boolean))
         (production :initial-identifier-character (:ordinary-initial-identifier-character) initial-identifier-character-ordinary
           (character-value ($default-action :ordinary-initial-identifier-character))
           (contains-escapes false))
         (production :initial-identifier-character (#\\ :hex-escape) initial-identifier-character-escape
           (character-value (if (is-ordinary-initial-identifier-character (character-value :hex-escape))
                              (character-value :hex-escape)
                              (bottom character)))
           (contains-escapes true)))
       
       (%charclass :ordinary-initial-identifier-character)
       
       (rule :continuing-identifier-character
             ((character-value character) (contains-escapes boolean))
         (production :continuing-identifier-character (:ordinary-continuing-identifier-character) continuing-identifier-character-ordinary
           (character-value ($default-action :ordinary-continuing-identifier-character))
           (contains-escapes false))
         (production :continuing-identifier-character (#\\ :hex-escape) continuing-identifier-character-escape
           (character-value (if (is-ordinary-continuing-identifier-character (character-value :hex-escape))
                              (character-value :hex-escape)
                              (bottom character)))
           (contains-escapes true)))
       
       (%charclass :ordinary-continuing-identifier-character)
       (%print-actions)
       
       (define reserved-words-r-e (vector string)
         (vector "abstract" "break" "case" "catch" "class" "const" "continue" "debugger" "default" "delete" "do" "else" "enum"
                 "eval" "export" "extends" "field" "final" "finally" "for" "function" "goto" "if" "implements" "import" "in"
                 "instanceof" "native" "new" "package" "private" "protected" "public" "return" "static" "switch" "synchronized"
                 "throw" "throws" "transient" "try" "typeof" "var" "volatile" "while" "with"))
       (define reserved-words-div (vector string)
         (vector "false" "null" "super" "this" "true"))
       (define non-reserved-words (vector string)
         (vector "constructor" "getter" "method" "override" "setter" "traditional" "version"))
       (define keywords (vector string)
         (append reserved-words-r-e (append reserved-words-div non-reserved-words)))
       
       (define (member (id string) (list (vector string))) boolean
         (if (empty list)
           false
           (if (string-equal id (nth list 0))
             true
             (member id (subseq list 1)))))
       
       (rule :identifier-or-reserved-word
             ((token token) (reg-exp-may-follow boolean))
         (production :identifier-or-reserved-word (:identifier-name) identifier-or-reserved-word-identifier-name
           (token (let ((id string (name :identifier-name)))
                    (if (and (member id keywords) (not (contains-escapes :identifier-name)))
                      (oneof keyword id)
                      (oneof identifier id))))
           (reg-exp-may-follow (let ((id string (name :identifier-name)))
                                 (and (member id reserved-words-r-e) (not (contains-escapes :identifier-name)))))))
       (%print-actions)
       
       (%section "Punctuators")
       
       (rule :punctuator
             ((token token) (reg-exp-may-follow boolean))
         (production :punctuator (:punctuator-r-e) punctuator-r-e
           (token (oneof punctuator (punctuator :punctuator-r-e)))
           (reg-exp-may-follow true))
         (production :punctuator (:punctuator-div) punctuator-div
           (token (oneof punctuator (punctuator :punctuator-div)))
           (reg-exp-may-follow false)))
       
       (rule :punctuator-r-e ((punctuator string))
         (production :punctuator-r-e (#\!) punctuator-not (punctuator "!"))
         (production :punctuator-r-e (#\! #\=) punctuator-not-equal (punctuator "!="))
         (production :punctuator-r-e (#\! #\= #\=) punctuator-not-identical (punctuator "!=="))
         (production :punctuator-r-e (#\#) punctuator-hash (punctuator "#"))
         (production :punctuator-r-e (#\%) punctuator-modulo (punctuator "%"))
         (production :punctuator-r-e (#\% #\=) punctuator-modulo-equals (punctuator "%="))
         (production :punctuator-r-e (#\&) punctuator-and (punctuator "&"))
         (production :punctuator-r-e (#\& #\&) punctuator-logical-and (punctuator "&&"))
         (production :punctuator-r-e (#\& #\& #\=) punctuator-logical-and-equals (punctuator "&&="))
         (production :punctuator-r-e (#\& #\=) punctuator-and-equals (punctuator "&="))
         (production :punctuator-r-e (#\() punctuator-open-parenthesis (punctuator "("))
         (production :punctuator-r-e (#\*) punctuator-times (punctuator "*"))
         (production :punctuator-r-e (#\* #\=) punctuator-times-equals (punctuator "*="))
         (production :punctuator-r-e (#\+) punctuator-plus (punctuator "+"))
         (production :punctuator-r-e (#\+ #\=) punctuator-plus-equals (punctuator "+="))
         (production :punctuator-r-e (#\,) punctuator-comma (punctuator ","))
         (production :punctuator-r-e (#\-) punctuator-minus (punctuator "-"))
         (production :punctuator-r-e (#\- #\=) punctuator-minus-equals (punctuator "-="))
         (production :punctuator-r-e (#\- #\>) punctuator-arrow (punctuator "->"))
         (production :punctuator-r-e (#\.) punctuator-period (punctuator "."))
         (production :punctuator-r-e (#\. #\.) punctuator-double-dot (punctuator ".."))
         (production :punctuator-r-e (#\. #\. #\.) punctuator-triple-dot (punctuator "..."))
         (production :punctuator-r-e (#\:) punctuator-colon (punctuator ":"))
         (production :punctuator-r-e (#\: #\:) punctuator-namespace (punctuator "::"))
         (production :punctuator-r-e (#\;) punctuator-semicolon (punctuator ";"))
         (production :punctuator-r-e (#\<) punctuator-less-than (punctuator "<"))
         (production :punctuator-r-e (#\< #\<) punctuator-left-shift (punctuator "<<"))
         (production :punctuator-r-e (#\< #\< #\=) punctuator-left-shift-equals (punctuator "<<="))
         (production :punctuator-r-e (#\< #\=) punctuator-less-than-or-equal (punctuator "<="))
         (production :punctuator-r-e (#\=) punctuator-assignment (punctuator "="))
         (production :punctuator-r-e (#\= #\=) punctuator-equal (punctuator "=="))
         (production :punctuator-r-e (#\= #\= #\=) punctuator-identical (punctuator "==="))
         (production :punctuator-r-e (#\>) punctuator-greater-than (punctuator ">"))
         (production :punctuator-r-e (#\> #\=) punctuator-greater-than-or-equal (punctuator ">="))
         (production :punctuator-r-e (#\> #\>) punctuator-right-shift (punctuator ">>"))
         (production :punctuator-r-e (#\> #\> #\=) punctuator-right-shift-equals (punctuator ">>="))
         (production :punctuator-r-e (#\> #\> #\>) punctuator-logical-right-shift (punctuator ">>>"))
         (production :punctuator-r-e (#\> #\> #\> #\=) punctuator-logical-right-shift-equals (punctuator ">>>="))
         (production :punctuator-r-e (#\?) punctuator-question (punctuator "?"))
         (production :punctuator-r-e (#\@) punctuator-at (punctuator "@"))
         (production :punctuator-r-e (#\[) punctuator-open-bracket (punctuator "["))
         (production :punctuator-r-e (#\^) punctuator-xor (punctuator "^"))
         (production :punctuator-r-e (#\^ #\=) punctuator-xor-equals (punctuator "^="))
         (production :punctuator-r-e (#\^ #\^) punctuator-logical-xor (punctuator "^^"))
         (production :punctuator-r-e (#\^ #\^ #\=) punctuator-logical-xor-equals (punctuator "^^="))
         (production :punctuator-r-e (#\{) punctuator-open-brace (punctuator "{"))
         (production :punctuator-r-e (#\|) punctuator-or (punctuator "|"))
         (production :punctuator-r-e (#\| #\=) punctuator-or-equals (punctuator "|="))
         (production :punctuator-r-e (#\| #\|) punctuator-logical-or (punctuator "||"))
         (production :punctuator-r-e (#\| #\| #\=) punctuator-logical-or-equals (punctuator "||="))
         (production :punctuator-r-e (#\~) punctuator-complement (punctuator "~")))
       
       (rule :punctuator-div ((punctuator string))
         (production :punctuator-div (#\)) punctuator-close-parenthesis (punctuator ")"))
         (production :punctuator-div (#\+ #\+) punctuator-increment (punctuator "++"))
         (production :punctuator-div (#\- #\-) punctuator-decrement (punctuator "--"))
         (production :punctuator-div (#\]) punctuator-close-bracket (punctuator "]"))
         (production :punctuator-div (#\}) punctuator-close-brace (punctuator "}")))
       
       (rule :division-punctuator ((punctuator string))
         (production :division-punctuator (#\/) punctuator-divide (punctuator "/"))
         (production :division-punctuator (#\/ #\=) punctuator-divide-equals (punctuator "/=")))
       (%print-actions)
       
       (%section "Numeric literals")
       
       (rule :numeric-literal ((double-value double))
         (production :numeric-literal (:decimal-literal) numeric-literal-decimal
           (double-value (rational-to-double (rational-value :decimal-literal))))
         (production :numeric-literal (:hex-integer-literal (:- :hex-digit)) numeric-literal-hex
           (double-value (rational-to-double (integer-to-rational (integer-value :hex-integer-literal)))))
         (production :numeric-literal (:octal-integer-literal) numeric-literal-octal
           (double-value (rational-to-double (integer-to-rational (integer-value :octal-integer-literal))))))
       (%print-actions)
       
       (define (expt (base rational) (exponent integer)) rational
         (if (= exponent 0)
           (integer-to-rational 1)
           (if (< exponent 0)
             (rational/ (integer-to-rational 1) (expt base (neg exponent)))
             (rational* base (expt base (- exponent 1))))))
       
       (rule :decimal-literal ((rational-value rational))
         (production :decimal-literal (:mantissa) decimal-literal
           (rational-value (rational-value :mantissa)))
         (production :decimal-literal (:mantissa :letter-e :signed-integer) decimal-literal-exponent
           (rational-value (rational* (rational-value :mantissa) (expt (integer-to-rational 10) (integer-value :signed-integer))))))
       
       (%charclass :letter-e)
       
       (rule :mantissa ((rational-value rational))
         (production :mantissa (:decimal-integer-literal) mantissa-integer
           (rational-value (integer-to-rational (integer-value :decimal-integer-literal))))
         (production :mantissa (:decimal-integer-literal #\.) mantissa-integer-dot
           (rational-value (integer-to-rational (integer-value :decimal-integer-literal))))
         (production :mantissa (:decimal-integer-literal #\. :fraction) mantissa-integer-dot-fraction
           (rational-value (rational+ (integer-to-rational (integer-value :decimal-integer-literal))
                                      (rational-value :fraction))))
         (production :mantissa (#\. :fraction) mantissa-dot-fraction
           (rational-value (rational-value :fraction))))
       
       (rule :decimal-integer-literal ((integer-value integer))
         (production :decimal-integer-literal (#\0) decimal-integer-literal-0
           (integer-value 0))
         (production :decimal-integer-literal (:non-zero-decimal-digits) decimal-integer-literal-nonzero
           (integer-value (integer-value :non-zero-decimal-digits))))
       
       (rule :non-zero-decimal-digits ((integer-value integer))
         (production :non-zero-decimal-digits (:non-zero-digit) non-zero-decimal-digits-first
           (integer-value (decimal-value :non-zero-digit)))
         (production :non-zero-decimal-digits (:non-zero-decimal-digits :a-s-c-i-i-digit) non-zero-decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :non-zero-decimal-digits)) (decimal-value :a-s-c-i-i-digit)))))
       
       (%charclass :non-zero-digit)
       
       (rule :fraction ((rational-value rational))
         (production :fraction (:decimal-digits) fraction-decimal-digits
           (rational-value (rational/ (integer-to-rational (integer-value :decimal-digits))
                                      (expt (integer-to-rational 10) (n-digits :decimal-digits))))))
       (%print-actions)
       
       (rule :signed-integer ((integer-value integer))
         (production :signed-integer (:decimal-digits) signed-integer-no-sign
           (integer-value (integer-value :decimal-digits)))
         (production :signed-integer (#\+ :decimal-digits) signed-integer-plus
           (integer-value (integer-value :decimal-digits)))
         (production :signed-integer (#\- :decimal-digits) signed-integer-minus
           (integer-value (neg (integer-value :decimal-digits)))))
       (%print-actions)
       
       (rule :decimal-digits
             ((integer-value integer) (n-digits integer))
         (production :decimal-digits (:a-s-c-i-i-digit) decimal-digits-first
           (integer-value (decimal-value :a-s-c-i-i-digit))
           (n-digits 1))
         (production :decimal-digits (:decimal-digits :a-s-c-i-i-digit) decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :decimal-digits)) (decimal-value :a-s-c-i-i-digit)))
           (n-digits (+ (n-digits :decimal-digits) 1))))
       (%print-actions)
       
       (rule :hex-integer-literal ((integer-value integer))
         (production :hex-integer-literal (#\0 :letter-x :hex-digit) hex-integer-literal-first
           (integer-value (hex-value :hex-digit)))
         (production :hex-integer-literal (:hex-integer-literal :hex-digit) hex-integer-literal-rest
           (integer-value (+ (* 16 (integer-value :hex-integer-literal)) (hex-value :hex-digit)))))
       (%charclass :letter-x)
       (%charclass :hex-digit)
       
       (rule :octal-integer-literal ((integer-value integer))
         (production :octal-integer-literal (#\0 :octal-digit) octal-integer-literal-first
           (integer-value (octal-value :octal-digit)))
         (production :octal-integer-literal (:octal-integer-literal :octal-digit) octal-integer-literal-rest
           (integer-value (+ (* 8 (integer-value :octal-integer-literal)) (octal-value :octal-digit)))))
       (%charclass :octal-digit)
       (%print-actions)
       
       (%section "Quantity literals")
       
       (rule :quantity-literal ((quantity-value quantity))
         (production :quantity-literal (:numeric-literal :quantity-name) quantity-literal-quantity-name
           (quantity-value (tuple quantity (double-value :numeric-literal) (name :quantity-name)))))
       
       (rule :quantity-name ((name string))
         (production :quantity-name ((:- :letter-e :letter-x) :identifier-name) quantity-name-identifier
           (name (name :identifier-name)))
         ;(production :quantity-name (:letter-x :identifier-name) quantity-name-x-identifier
         ;  (name (append (vector ($default-action :letter-x)) (name :identifier-name))))
         )
       (%print-actions)
       
       (%section "String literals")
       
       (grammar-argument :theta single double)
       (rule :string-literal ((string-value string))
         (production :string-literal (#\' (:string-chars single) #\') string-literal-single
           (string-value (string-value :string-chars)))
         (production :string-literal (#\" (:string-chars double) #\") string-literal-double
           (string-value (string-value :string-chars))))
       (%print-actions)
       
       (rule (:string-chars :theta) ((string-value string))
         (production (:string-chars :theta) () string-chars-none
           (string-value ""))
         (production (:string-chars :theta) ((:string-chars :theta) (:string-char :theta)) string-chars-some
           (string-value (append (string-value :string-chars)
                                 (vector (character-value :string-char))))))
       
       (rule (:string-char :theta) ((character-value character))
         (production (:string-char :theta) ((:literal-string-char :theta)) string-char-literal
           (character-value ($default-action :literal-string-char)))
         (production (:string-char :theta) (#\\ :string-escape) string-char-escape
           (character-value (character-value :string-escape))))
       
       (%charclass (:literal-string-char single))
       (%charclass (:literal-string-char double))
       (%print-actions)
       
       (rule :string-escape ((character-value character))
         (production :string-escape (:control-escape) string-escape-control
           (character-value (character-value :control-escape)))
         (production :string-escape (:octal-escape) string-escape-octal
           (character-value (character-value :octal-escape)))
         (production :string-escape (:hex-escape) string-escape-hex
           (character-value (character-value :hex-escape)))
         (production :string-escape (:identity-escape) string-escape-non-escape
           (character-value ($default-action :identity-escape))))
       (%charclass :identity-escape)
       (%print-actions)
       
       (rule :control-escape ((character-value character))
         (production :control-escape (#\b) control-escape-backspace (character-value #?0008))
         (production :control-escape (#\f) control-escape-form-feed (character-value #?000C))
         (production :control-escape (#\n) control-escape-new-line (character-value #?000A))
         (production :control-escape (#\r) control-escape-return (character-value #?000D))
         (production :control-escape (#\t) control-escape-tab (character-value #?0009))
         (production :control-escape (#\v) control-escape-vertical-tab (character-value #?000B)))
       (%print-actions)
       
       (rule :octal-escape ((character-value character))
         (production :octal-escape (:octal-digit (:- :octal-digit)) octal-escape-1
           (character-value (code-to-character (octal-value :octal-digit))))
         (production :octal-escape (:zero-to-three :octal-digit (:- :octal-digit)) octal-escape-2-low
           (character-value (code-to-character (+ (* 8 (octal-value :zero-to-three))
                                                  (octal-value :octal-digit)))))
         (production :octal-escape (:four-to-seven :octal-digit) octal-escape-2-high
           (character-value (code-to-character (+ (* 8 (octal-value :four-to-seven))
                                                  (octal-value :octal-digit)))))
         (production :octal-escape (:zero-to-three :octal-digit :octal-digit) octal-escape-3
           (character-value (code-to-character (+ (+ (* 64 (octal-value :zero-to-three))
                                                     (* 8 (octal-value :octal-digit 1)))
                                                  (octal-value :octal-digit 2))))))
       (%charclass :zero-to-three)
       (%charclass :four-to-seven)
       (%print-actions)
       
       (rule :hex-escape ((character-value character))
         (production :hex-escape (#\x :hex-digit :hex-digit) hex-escape-2
           (character-value (code-to-character (+ (* 16 (hex-value :hex-digit 1))
                                                  (hex-value :hex-digit 2)))))
         (production :hex-escape (#\u :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-4
           (character-value (code-to-character (+ (+ (+ (* 4096 (hex-value :hex-digit 1))
                                                        (* 256 (hex-value :hex-digit 2)))
                                                     (* 16 (hex-value :hex-digit 3)))
                                                  (hex-value :hex-digit 4))))))
       (%print-actions)
       
       (%section "Regular expression literals")
       
       (grammar-argument :rho slash guillemet)
       
       (rule (:reg-exp-literal :rho) ((r-e-value reg-exp))
         (production (:reg-exp-literal :rho) ((:reg-exp-body :rho) :reg-exp-flags) reg-exp-literal
           (r-e-value (tuple reg-exp (r-e-body :reg-exp-body) (r-e-flags :reg-exp-flags)))))
       
       (rule :reg-exp-flags ((r-e-flags string))
         (production :reg-exp-flags () reg-exp-flags-none
           (r-e-flags ""))
         (production :reg-exp-flags (:reg-exp-flags :continuing-identifier-character) reg-exp-flags-more
           (r-e-flags (append (r-e-flags :reg-exp-flags) (vector (character-value :continuing-identifier-character))))))
       
       (rule (:reg-exp-body :rho) ((r-e-body string))
         (production (:reg-exp-body slash) (#\/ :reg-exp-first-char (:reg-exp-chars slash) #\/) reg-exp-body-slash
           (r-e-body (append (r-e-body :reg-exp-first-char)
                             (r-e-body :reg-exp-chars))))
         (production (:reg-exp-body guillemet) (#?00AB (:reg-exp-chars guillemet) #?00BB) reg-exp-body-guillemet
           (r-e-body (r-e-body :reg-exp-chars))))
       
       (rule :reg-exp-first-char ((r-e-body string))
         (production :reg-exp-first-char (:ordinary-reg-exp-first-char) reg-exp-first-char-ordinary
           (r-e-body (vector ($default-action :ordinary-reg-exp-first-char))))
         (production :reg-exp-first-char (#\\ :non-terminator) reg-exp-first-char-escape
           (r-e-body (vector #\\ ($default-action :non-terminator)))))
       
       (%charclass :ordinary-reg-exp-first-char)
       
       (rule (:reg-exp-chars :rho) ((r-e-body string))
         (production (:reg-exp-chars :rho) () reg-exp-chars-none
           (r-e-body ""))
         (production (:reg-exp-chars :rho) ((:reg-exp-chars :rho) (:reg-exp-char :rho)) reg-exp-chars-more
           (r-e-body (append (r-e-body :reg-exp-chars)
                             (r-e-body :reg-exp-char)))))
       
       (rule (:reg-exp-char :rho) ((r-e-body string))
         (production (:reg-exp-char :rho) ((:ordinary-reg-exp-char :rho)) reg-exp-char-ordinary
           (r-e-body (vector ($default-action :ordinary-reg-exp-char))))
         (production (:reg-exp-char :rho) (#\\ :non-terminator) reg-exp-char-escape
           (r-e-body (vector #\\ ($default-action :non-terminator)))))
       
       (%charclass (:ordinary-reg-exp-char slash))
       (%charclass (:ordinary-reg-exp-char guillemet))
       )))
  
  (defparameter *ll* (world-lexer *lw* 'code-lexer))
  (defparameter *lg* (lexer-grammar *ll*))
  (set-up-lexer-metagrammar *ll*)
  (defparameter *lm* (lexer-metagrammar *ll*)))

#|
(depict-rtf-to-local-file
 ";JS20;LexerCharClasses.rtf"
 "JavaScript 2 Lexer Character Classes"
 #'(lambda (rtf-stream)
     (depict-paragraph (rtf-stream ':grammar-header)
       (depict rtf-stream "Character Classes"))
     (dolist (charclass (lexer-charclasses *ll*))
       (depict-charclass rtf-stream charclass))
     (depict-paragraph (rtf-stream ':grammar-header)
       (depict rtf-stream "Grammar"))
     (depict-grammar rtf-stream *lg*)))

(progn
  (depict-rtf-to-local-file
   ";JS20;LexerGrammar.rtf"
   "JavaScript 2 Lexer Grammar"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw* :visible-semantics nil)))
  (depict-rtf-to-local-file
   ";JS20;LexerSemantics.rtf"
   "JavaScript 2 Lexer Semantics"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw*))))

(progn
  (depict-html-to-local-file
   ";JS20;LexerGrammar.html"
   "JavaScript 2 Lexer Grammar"
   t
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw* :visible-semantics nil))
   :external-link-base "notation-semantic.html")
  (depict-html-to-local-file
   ";JS20;LexerSemantics.html"
   "JavaScript 2 Lexer Semantics"
   t
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw*))
   :external-link-base "notation-semantic.html"))

(with-local-output (s ";JS20;LexerGrammar.txt") (print-lexer *ll* s) (print-grammar *lg* s))

(print-illegal-strings m)
|#


(defun js-state-transition (action-results)
  (assert-type action-results (tuple t bool))
  (values action-results (if (second action-results) '($re) '($non-re))))

(defun js-metaparse (string &key trace)
  (lexer-metaparse *ll* string :initial-state '($re) :state-transition #'js-state-transition :trace trace))

(defun js-pmetaparse (string &key (stream t) trace)
  (lexer-pmetaparse *ll* string :initial-state '($re) :state-transition #'js-state-transition :stream stream :trace trace))


; Return the JavaScript input string as a list of tokens like:
;   (($number . 3.0) + - ++ else ($string . "a+bgde") ($end))
(defun tokenize (string)
  (mapcar
   #'(lambda (token-value)
       (let ((token-value (car token-value)))
         (ecase (car token-value)
           (identifier (cons '$identifier (cdr token-value)))
           ((keyword punctuator) (intern (string-upcase (cdr token-value))))
           (number (cons '$number (cdr token-value)))
           (string (cons '$string (cdr token-value)))
           (regular-expression (cons '$regular-expression (cdr token-value)))
           (end '($end)))))
   (js-metaparse string)))


#|
(lexer-pparse *ll* "0x20")
(lexer-pparse *ll* "2b")
(lexer-pparse *ll* " 3.75" :trace t)
(lexer-pparse *ll* "25" :trace :code)
(js-pmetaparse "32+abc//23e-a4*7e-2 3 id4 4ef;")
(js-pmetaparse "32+abc//23e-a4*7e-2 3 id4 4ef;
")
(js-pmetaparse "32+abc/ /23e-a4*7e-2 3 /*id4 4*-/ef;

fjds*/y//z")
(js-pmetaparse "3a+in'a+b\\147\"de'\"'\"")
(js-pmetaparse "3*/regexp*///x")
(js-pmetaparse "/regexp*///x")
(js-pmetaparse "if \\x69f \\u0069f")
(js-pmetaparse "if \\x69f z\\x20z")
(js-pmetaparse "3lbs 3in 3 in 3_in 3_lbs")
(js-pmetaparse "3a+in'a+b\\040\\077\\700\\150\\15A\\69\"de'\"'\"")
|#

(length (grammar-states *lg*))
