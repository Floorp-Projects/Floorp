;;;
;;; JavaScript 2.0 lexer
;;;
;;; Waldemar Horwat (waldemar@acm.org)
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
               (:non-terminator-or-slash (- :non-terminator (#\/)) ())
               (:non-terminator-or-asterisk-or-slash (- :non-terminator (#\* #\/)) ())
               (:ordinary-initial-identifier-character (+ :unicode-initial-alphabetic (#\$ #\_))
                                                       (($default-action $default-action)))
               (:ordinary-continuing-identifier-character (+ :unicode-alphanumeric (#\$ #\_))
                                                          (($default-action $default-action)))
               (:a-s-c-i-i-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                 (($default-action $default-action)
                                  (decimal-value $digit-value)))
               (:non-zero-digit (#\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                ((decimal-value $digit-value)))
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
               (:ordinary-reg-exp-char (- :non-terminator (#\\ #\/))
                                       (($default-action $default-action))))
              (($default-action character nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       (rule :$next-token
             ((token token))
         (production :$next-token ($unit (:next-token unit)) $next-token-unit
           (token (token :next-token)))
         (production :$next-token ($re (:next-token re)) $next-token-re
           (token (token :next-token)))
         (production :$next-token ($non-re (:next-token div)) $next-token-non-re
           (token (token :next-token))))
       
       (%text nil "The start symbols are: "
              (:grammar-symbol (:next-token unit)) " if the previous token was a number; "
              (:grammar-symbol (:next-token re)) " if the previous token was not a number and a "
              (:character-literal #\/) " should be interpreted as a regular expression; and "
              (:grammar-symbol (:next-token div)) " if the previous token was not a number and a "
              (:character-literal #\/) " should be interpreted as a division or division-assignment operator.")

       (deftype semantic-exception (oneof syntax-error))
       
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
       
       (production :single-line-block-comment (#\/ #\* :block-comment-characters #\* #\/) single-line-block-comment)
       
       (production :block-comment-characters () block-comment-characters-empty)
       (production :block-comment-characters (:block-comment-characters :non-terminator-or-slash) block-comment-characters-chars)
       (production :block-comment-characters (:pre-slash-characters #\/) block-comment-characters-slash)
       
       (production :pre-slash-characters () pre-slash-characters-empty)
       (production :pre-slash-characters (:block-comment-characters :non-terminator-or-asterisk-or-slash) pre-slash-characters-chars)
       (production :pre-slash-characters (:pre-slash-characters #\/) pre-slash-characters-slash)
       
       (%charclass :non-terminator-or-slash)
       (%charclass :non-terminator-or-asterisk-or-slash)
       
       (production :multi-line-block-comment (#\/ #\* :multi-line-block-comment-characters :block-comment-characters #\* #\/) multi-line-block-comment)
       
       (production :multi-line-block-comment-characters (:block-comment-characters :line-terminator) multi-line-block-comment-characters-first)
       (production :multi-line-block-comment-characters (:multi-line-block-comment-characters :block-comment-characters :line-terminator)
                   multi-line-block-comment-characters-rest)
       (%print-actions)
       
       (%section "White space")
       
       (production :white-space () white-space-empty)
       (production :white-space (:white-space :white-space-character) white-space-character)
       (production :white-space (:white-space :single-line-block-comment) white-space-single-line-block-comment)
       
       (%section "Line breaks")
       
       (production :line-break (:line-terminator) line-break-line-terminator)
       (production :line-break (:line-comment :line-terminator) line-break-line-comment)
       (production :line-break (:multi-line-block-comment) line-break-multi-line-block-comment)
       
       (production :line-breaks (:line-break) line-breaks-first)
       (production :line-breaks (:line-breaks :white-space :line-break) line-breaks-rest)
       
       (%section "Tokens")
       
       (grammar-argument :tau re div unit)
       (grammar-argument :tau_2 re div)
       
       (rule (:next-token :tau)
             ((token token))
         (production (:next-token re) (:white-space (:token re)) next-token-re
           (token (token :token)))
         (production (:next-token div) (:white-space (:token div)) next-token-div
           (token (token :token)))
         (production (:next-token unit) ((:- :ordinary-continuing-identifier-character #\\) :white-space (:token div)) next-token-unit-normal
           (token (token :token)))
         (production (:next-token unit) ((:- #\_) :identifier-name) next-token-unit-name
           (token (oneof string (name :identifier-name))))
         (production (:next-token unit) (#\_ :identifier-name) next-token-unit-underscore-name
           (token (oneof string (name :identifier-name)))))
       
       (%print-actions)
       
       (rule (:token :tau_2)
             ((token token))
         (production (:token :tau_2) (:line-breaks) token-line-breaks
           (token (oneof line-break)))
         (production (:token :tau_2) (:identifier-or-reserved-word) token-identifier-or-reserved-word
           (token (token :identifier-or-reserved-word)))
         (production (:token :tau_2) (:punctuator) token-punctuator
           (token (oneof punctuator (punctuator :punctuator))))
         (production (:token div) (:division-punctuator) token-division-punctuator
           (token (oneof punctuator (punctuator :division-punctuator))))
         (production (:token :tau_2) (:numeric-literal) token-numeric-literal
           (token (oneof number (double-value :numeric-literal))))
         (production (:token :tau_2) (:string-literal) token-string-literal
           (token (oneof string (string-value :string-literal))))
         (production (:token re) (:reg-exp-literal) token-reg-exp-literal
           (token (oneof regular-expression (r-e-value :reg-exp-literal))))
         (production (:token :tau_2) (:end-of-input) token-end
           (token (oneof end))))
       
       (production :end-of-input ($end) end-of-input-end)
       (production :end-of-input (:line-comment $end) end-of-input-line-comment)
       
       (deftype reg-exp (tuple (re-body string)
                          (re-flags string)))
       
       (deftype quantity (tuple (amount double)
                           (unit string)))
       
       (deftype token (oneof line-break
                             (identifier string)
                             (keyword string)
                             (punctuator string)
                             (number double)
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
         (production :identifier-name (:null-escapes :initial-identifier-character) identifier-name-initial-null-escapes
           (name (vector (character-value :initial-identifier-character)))
           (contains-escapes true))
         (production :identifier-name (:identifier-name :continuing-identifier-character) identifier-name-continuing
           (name (append (name :identifier-name) (vector (character-value :continuing-identifier-character))))
           (contains-escapes (or (contains-escapes :identifier-name)
                                 (contains-escapes :continuing-identifier-character))))
         (production :identifier-name (:identifier-name :null-escape) identifier-name-null-escape
           (name (name :identifier-name))
           (contains-escapes true)))

       (production :null-escapes (:null-escape) null-escapes-one)
       (production :null-escapes (:null-escapes :null-escape) null-escapes-more)
       
       (production :null-escape (#\\ #\Q) null-escape-q)

       (rule :initial-identifier-character
             ((character-value character) (contains-escapes boolean))
         (production :initial-identifier-character (:ordinary-initial-identifier-character) initial-identifier-character-ordinary
           (character-value ($default-action :ordinary-initial-identifier-character))
           (contains-escapes false))
         (production :initial-identifier-character (#\\ :hex-escape) initial-identifier-character-escape
           (character-value (if (is-ordinary-initial-identifier-character (character-value :hex-escape))
                              (character-value :hex-escape)
                              (throw (oneof syntax-error))))
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
                              (throw (oneof syntax-error))))
           (contains-escapes true)))
       
       (%charclass :ordinary-continuing-identifier-character)
       (%print-actions)
       
       (define reserved-words (vector string)
         (vector "abstract" "break" "case" "catch" "class" "const" "constructor" "continue" "debugger" "default" "delete" "do" "else" "enum"
                 "eval" "export" "extends" "false" "final" "finally" "for" "function" "goto" "if" "implements" "import" "in" "include"
                 "instanceof" "interface" "namespace" "native" "new" "null" "package" "private" "protected" "public" "return" "static" "super"
                 "switch" "synchronized" "this" "throw" "throws" "transient" "true" "try" "typeof" "use" "var" "volatile" "while" "with"))
       (define non-reserved-words (vector string)
         (vector "get" "set"))
       (define keywords (vector string)
         (append reserved-words non-reserved-words))
       
       (define (member (id string) (list (vector string))) boolean
         (if (empty list)
           false
           (if (string-equal id (nth list 0))
             true
             (member id (subseq list 1)))))
       
       (rule :identifier-or-reserved-word
             ((token token))
         (production :identifier-or-reserved-word (:identifier-name) identifier-or-reserved-word-identifier-name
           (token (let ((id string (name :identifier-name)))
                    (if (and (member id keywords) (not (contains-escapes :identifier-name)))
                      (oneof keyword id)
                      (oneof identifier id))))))
       (%print-actions)
       
       (%section "Punctuators")
       
       (rule :punctuator ((punctuator string))
         (production :punctuator (#\!) punctuator-not (punctuator "!"))
         (production :punctuator (#\! #\=) punctuator-not-equal (punctuator "!="))
         (production :punctuator (#\! #\= #\=) punctuator-not-identical (punctuator "!=="))
         (production :punctuator (#\#) punctuator-hash (punctuator "#"))
         (production :punctuator (#\%) punctuator-modulo (punctuator "%"))
         (production :punctuator (#\% #\=) punctuator-modulo-equals (punctuator "%="))
         (production :punctuator (#\&) punctuator-and (punctuator "&"))
         (production :punctuator (#\& #\&) punctuator-logical-and (punctuator "&&"))
         (production :punctuator (#\& #\& #\=) punctuator-logical-and-equals (punctuator "&&="))
         (production :punctuator (#\& #\=) punctuator-and-equals (punctuator "&="))
         (production :punctuator (#\() punctuator-open-parenthesis (punctuator "("))
         (production :punctuator (#\)) punctuator-close-parenthesis (punctuator ")"))
         (production :punctuator (#\*) punctuator-times (punctuator "*"))
         (production :punctuator (#\* #\=) punctuator-times-equals (punctuator "*="))
         (production :punctuator (#\+) punctuator-plus (punctuator "+"))
         (production :punctuator (#\+ #\+) punctuator-increment (punctuator "++"))
         (production :punctuator (#\+ #\=) punctuator-plus-equals (punctuator "+="))
         (production :punctuator (#\,) punctuator-comma (punctuator ","))
         (production :punctuator (#\-) punctuator-minus (punctuator "-"))
         (production :punctuator (#\- #\-) punctuator-decrement (punctuator "--"))
         (production :punctuator (#\- #\=) punctuator-minus-equals (punctuator "-="))
         (production :punctuator (#\- #\>) punctuator-arrow (punctuator "->"))
         (production :punctuator (#\.) punctuator-dot (punctuator "."))
         (production :punctuator (#\. #\.) punctuator-double-dot (punctuator ".."))
         (production :punctuator (#\. #\. #\.) punctuator-triple-dot (punctuator "..."))
         (production :punctuator (#\:) punctuator-colon (punctuator ":"))
         (production :punctuator (#\: #\:) punctuator-namespace (punctuator "::"))
         (production :punctuator (#\;) punctuator-semicolon (punctuator ";"))
         (production :punctuator (#\<) punctuator-less-than (punctuator "<"))
         (production :punctuator (#\< #\<) punctuator-left-shift (punctuator "<<"))
         (production :punctuator (#\< #\< #\=) punctuator-left-shift-equals (punctuator "<<="))
         (production :punctuator (#\< #\=) punctuator-less-than-or-equal (punctuator "<="))
         (production :punctuator (#\=) punctuator-assignment (punctuator "="))
         (production :punctuator (#\= #\=) punctuator-equal (punctuator "=="))
         (production :punctuator (#\= #\= #\=) punctuator-identical (punctuator "==="))
         (production :punctuator (#\>) punctuator-greater-than (punctuator ">"))
         (production :punctuator (#\> #\=) punctuator-greater-than-or-equal (punctuator ">="))
         (production :punctuator (#\> #\>) punctuator-right-shift (punctuator ">>"))
         (production :punctuator (#\> #\> #\=) punctuator-right-shift-equals (punctuator ">>="))
         (production :punctuator (#\> #\> #\>) punctuator-logical-right-shift (punctuator ">>>"))
         (production :punctuator (#\> #\> #\> #\=) punctuator-logical-right-shift-equals (punctuator ">>>="))
         (production :punctuator (#\?) punctuator-question (punctuator "?"))
         (production :punctuator (#\@) punctuator-at (punctuator "@"))
         (production :punctuator (#\[) punctuator-open-bracket (punctuator "["))
         (production :punctuator (#\]) punctuator-close-bracket (punctuator "]"))
         (production :punctuator (#\^) punctuator-xor (punctuator "^"))
         (production :punctuator (#\^ #\=) punctuator-xor-equals (punctuator "^="))
         (production :punctuator (#\^ #\^) punctuator-logical-xor (punctuator "^^"))
         (production :punctuator (#\^ #\^ #\=) punctuator-logical-xor-equals (punctuator "^^="))
         (production :punctuator (#\{) punctuator-open-brace (punctuator "{"))
         (production :punctuator (#\|) punctuator-or (punctuator "|"))
         (production :punctuator (#\| #\=) punctuator-or-equals (punctuator "|="))
         (production :punctuator (#\| #\|) punctuator-logical-or (punctuator "||"))
         (production :punctuator (#\| #\| #\=) punctuator-logical-or-equals (punctuator "||="))
         (production :punctuator (#\}) punctuator-close-brace (punctuator "}"))
         (production :punctuator (#\~) punctuator-complement (punctuator "~")))
       
       (rule :division-punctuator ((punctuator string))
         (production :division-punctuator (#\/ (:- #\/ #\*)) punctuator-divide (punctuator "/"))
         (production :division-punctuator (#\/ #\=) punctuator-divide-equals (punctuator "/=")))
       (%print-actions)
       
       (%section "Numeric literals")
       
       (rule :numeric-literal ((double-value double))
         (production :numeric-literal (:decimal-literal) numeric-literal-decimal
           (double-value (rational-to-double (rational-value :decimal-literal))))
         (production :numeric-literal (:hex-integer-literal (:- :hex-digit)) numeric-literal-hex
           (double-value (rational-to-double (integer-value :hex-integer-literal)))))
       (%print-actions)
       
       (define (expt (base rational) (exponent integer)) rational
         (if (= exponent 0)
           1
           (if (< exponent 0)
             (rational/ 1 (expt base (neg exponent)))
             (rational* base (expt base (- exponent 1))))))
       
       (rule :decimal-literal ((rational-value rational))
         (production :decimal-literal (:mantissa) decimal-literal
           (rational-value (rational-value :mantissa)))
         (production :decimal-literal (:mantissa :letter-e :signed-integer) decimal-literal-exponent
           (rational-value (rational* (rational-value :mantissa) (expt 10 (integer-value :signed-integer))))))
       
       (%charclass :letter-e)
       
       (rule :mantissa ((rational-value rational))
         (production :mantissa (:decimal-integer-literal) mantissa-integer
           (rational-value (integer-value :decimal-integer-literal)))
         (production :mantissa (:decimal-integer-literal #\.) mantissa-integer-dot
           (rational-value (integer-value :decimal-integer-literal)))
         (production :mantissa (:decimal-integer-literal #\. :fraction) mantissa-integer-dot-fraction
           (rational-value (rational+ (integer-value :decimal-integer-literal)
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
           (rational-value (rational/ (integer-value :decimal-digits)
                                      (expt 10 (n-digits :decimal-digits))))))
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
                                 (vector (character-value :string-char)))))
         (production (:string-chars :theta) ((:string-chars :theta) :null-escape) string-chars-null-escape
           (string-value (string-value :string-chars))))
       
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
         (production :string-escape (:zero-escape) string-escape-zero
           (character-value (character-value :zero-escape)))
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
       
       (rule :zero-escape ((character-value character))
         (production :zero-escape (#\0 (:- :a-s-c-i-i-digit)) zero-escape-zero
           (character-value #?0000)))
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
       
       (rule :reg-exp-literal ((r-e-value reg-exp))
         (production :reg-exp-literal (:reg-exp-body :reg-exp-flags) reg-exp-literal
           (r-e-value (tuple reg-exp (r-e-body :reg-exp-body) (r-e-flags :reg-exp-flags)))))
       
       (rule :reg-exp-flags ((r-e-flags string))
         (production :reg-exp-flags () reg-exp-flags-none
           (r-e-flags ""))
         (production :reg-exp-flags (:reg-exp-flags :continuing-identifier-character) reg-exp-flags-more
           (r-e-flags (append (r-e-flags :reg-exp-flags) (vector (character-value :continuing-identifier-character)))))
         (production :reg-exp-flags (:reg-exp-flags :null-escape) reg-exp-flags-null-escape
           (r-e-flags (r-e-flags :reg-exp-flags))))
       
       (rule :reg-exp-body ((r-e-body string))
         (production :reg-exp-body (#\/ (:- #\*) :reg-exp-chars #\/) reg-exp-body
           (r-e-body (r-e-body :reg-exp-chars))))
       
       (rule :reg-exp-chars ((r-e-body string))
         (production :reg-exp-chars (:reg-exp-char) reg-exp-chars-one
           (r-e-body (r-e-body :reg-exp-char)))
         (production :reg-exp-chars (:reg-exp-chars :reg-exp-char) reg-exp-chars-more
           (r-e-body (append (r-e-body :reg-exp-chars)
                             (r-e-body :reg-exp-char)))))
       
       (rule :reg-exp-char ((r-e-body string))
         (production :reg-exp-char (:ordinary-reg-exp-char) reg-exp-char-ordinary
           (r-e-body (vector ($default-action :ordinary-reg-exp-char))))
         (production :reg-exp-char (#\\ :non-terminator) reg-exp-char-escape
           (r-e-body (vector #\\ ($default-action :non-terminator)))))
       
       (%charclass :ordinary-reg-exp-char)
       )))
  
  (defparameter *ll* (world-lexer *lw* 'code-lexer))
  (defparameter *lg* (lexer-grammar *ll*))
  (set-up-lexer-metagrammar *ll*)
  (defparameter *lm* (lexer-metagrammar *ll*)))


#|
(depict-rtf-to-local-file
 "JS20/LexerCharClasses.rtf"
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
   "JS20/LexerGrammar.rtf"
   "JavaScript 2 Lexer Grammar"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw* :visible-semantics nil)))
  (depict-rtf-to-local-file
   "JS20/LexerSemantics.rtf"
   "JavaScript 2 Lexer Semantics"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw*))))

(progn
  (depict-html-to-local-file
   "JS20/LexerGrammar.html"
   "JavaScript 2 Lexer Grammar"
   t
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw* :visible-semantics nil))
   :external-link-base "notation.html")
  (depict-html-to-local-file
   "JS20/LexerSemantics.html"
   "JavaScript 2 Lexer Semantics"
   t
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *lw*))
   :external-link-base "notation.html"))

(with-local-output (s "JS20/LexerGrammar.txt") (print-lexer *ll* s) (print-grammar *lg* s))

(print-illegal-strings m)
|#


#+allegro (clean-grammar *lg*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *lg*))
