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
              :$next-input-element
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
               (:initial-identifier-character (+ :unicode-initial-alphabetic (#\$ #\_))
                                              (($default-action $default-action)))
               (:continuing-identifier-character (+ :unicode-alphanumeric (#\$ #\_))
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
               (:identity-escape (- :non-terminator (+ (#\_) :unicode-alphanumeric))
                                 (($default-action $default-action)))
               (:ordinary-reg-exp-char (- :non-terminator (#\\ #\/))
                                       (($default-action $default-action))))
              (($default-action character nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       (rule :$next-input-element
             ((lex input-element))
         (production :$next-input-element ($unit (:next-input-element unit)) $next-input-element-unit
           (lex (lex :next-input-element)))
         (production :$next-input-element ($re (:next-input-element re)) $next-input-element-re
           (lex (lex :next-input-element)))
         (production :$next-input-element ($non-re (:next-input-element div)) $next-input-element-non-re
           (lex (lex :next-input-element))))
       
       (%text nil "The start symbols are: "
              (:grammar-symbol (:next-input-element unit)) " if the previous input element was a number; "
              (:grammar-symbol (:next-input-element re)) " if the previous input element was not a number and a "
              (:character-literal #\/) " should be interpreted as a regular expression; and "
              (:grammar-symbol (:next-input-element div)) " if the previous input element was not a number and a "
              (:character-literal #\/) " should be interpreted as a division or division-assignment operator.")
       
       (deftag line-break)
       (deftag end-of-input)
       
       (deftuple keyword (name string))
       (deftuple punctuator (name string))
       (deftuple identifier (name string))
       (deftuple number (value float64))
       (deftuple regular-expression (body string) (flags string))
       
       (deftype token (union keyword punctuator identifier number string regular-expression))
       (deftype input-element (union (tag line-break end-of-input) token))
       
       
       (deftag syntax-error)
       (deftype semantic-exception (tag syntax-error))
       
       (%heading 1 "Unicode Character Classes")
       (%charclass :unicode-character)
       (%charclass :unicode-initial-alphabetic)
       (%charclass :unicode-alphanumeric)
       (%charclass :white-space-character)
       (%charclass :line-terminator)
       (%charclass :a-s-c-i-i-digit)
       (%print-actions)
       
       (%heading 1 "Comments")
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
       
       (%heading 1 "White Space")
       
       (production :white-space () white-space-empty)
       (production :white-space (:white-space :white-space-character) white-space-character)
       (production :white-space (:white-space :single-line-block-comment) white-space-single-line-block-comment)
       
       (%heading 1 "Line Breaks")
       
       (production :line-break (:line-terminator) line-break-line-terminator)
       (production :line-break (:line-comment :line-terminator) line-break-line-comment)
       (production :line-break (:multi-line-block-comment) line-break-multi-line-block-comment)
       
       (production :line-breaks (:line-break) line-breaks-first)
       (production :line-breaks (:line-breaks :white-space :line-break) line-breaks-rest)
       
       (%heading 1 "Input Elements")
       
       (grammar-argument :nu re div unit)
       (grammar-argument :nu_2 re div)
       
       (rule (:next-input-element :nu)
             ((lex input-element))
         (production (:next-input-element re) (:white-space (:input-element re)) next-input-element-re
           (lex (lex :input-element)))
         (production (:next-input-element div) (:white-space (:input-element div)) next-input-element-div
           (lex (lex :input-element)))
         (production (:next-input-element unit) ((:- :continuing-identifier-character #\\) :white-space (:input-element div)) next-input-element-unit-normal
           (lex (lex :input-element)))
         (production (:next-input-element unit) ((:- #\_) :identifier-name) next-input-element-unit-name
           (lex (lex-name :identifier-name)))
         #|(production (:next-input-element unit) (#\_ :identifier-name) next-input-element-unit-underscore-name
           (lex (lex-name :identifier-name)))|#)
       
       (%print-actions)
       
       (rule (:input-element :nu_2)
             ((lex input-element))
         (production (:input-element :nu_2) (:line-breaks) input-element-line-breaks
           (lex line-break))
         (production (:input-element :nu_2) (:identifier-or-keyword) input-element-identifier-or-keyword
           (lex (lex :identifier-or-keyword)))
         (production (:input-element :nu_2) (:punctuator) input-element-punctuator
           (lex (lex :punctuator)))
         (production (:input-element div) (:division-punctuator) input-element-division-punctuator
           (lex (lex :division-punctuator)))
         (production (:input-element :nu_2) (:numeric-literal) input-element-numeric-literal
           (lex (lex :numeric-literal)))
         (production (:input-element :nu_2) (:string-literal) input-element-string-literal
           (lex (lex :string-literal)))
         (production (:input-element re) (:reg-exp-literal) input-element-reg-exp-literal
           (lex (lex :reg-exp-literal)))
         (production (:input-element :nu_2) (:end-of-input) input-element-end
           (lex end-of-input)))
       
       (production :end-of-input ($end) end-of-input-end)
       (production :end-of-input (:line-comment $end) end-of-input-line-comment)
       (%print-actions)
       
       (%heading 1 "Keywords and Identifiers")
       
       (rule :identifier-name
             ((lex-name string) (contains-escapes boolean))
         (production :identifier-name (:initial-identifier-character-or-escape) identifier-name-initial
           (lex-name (vector (lex-char :initial-identifier-character-or-escape)))
           (contains-escapes (contains-escapes :initial-identifier-character-or-escape)))
         (production :identifier-name (:null-escapes :initial-identifier-character-or-escape) identifier-name-initial-null-escapes
           (lex-name (vector (lex-char :initial-identifier-character-or-escape)))
           (contains-escapes true))
         (production :identifier-name (:identifier-name :continuing-identifier-character-or-escape) identifier-name-continuing
           (lex-name (append (lex-name :identifier-name) (vector (lex-char :continuing-identifier-character-or-escape))))
           (contains-escapes (or (contains-escapes :identifier-name)
                                 (contains-escapes :continuing-identifier-character-or-escape))))
         (production :identifier-name (:identifier-name :null-escape) identifier-name-null-escape
           (lex-name (lex-name :identifier-name))
           (contains-escapes true)))
       
       (production :null-escapes (:null-escape) null-escapes-one)
       (production :null-escapes (:null-escapes :null-escape) null-escapes-more)
       
       (production :null-escape (#\\ #\_) null-escape-underscore)
       
       (rule :initial-identifier-character-or-escape
             ((lex-char character) (contains-escapes boolean))
         (production :initial-identifier-character-or-escape (:initial-identifier-character) initial-identifier-character-or-escape-ordinary
           (lex-char ($default-action :initial-identifier-character))
           (contains-escapes false))
         (production :initial-identifier-character-or-escape (#\\ :hex-escape) initial-identifier-character-or-escape-escape
           (lex-char (begin (if (is-initial-identifier-character (lex-char :hex-escape))
                              (return (lex-char :hex-escape))
                              (throw syntax-error))))
           (contains-escapes true)))
       
       (%charclass :initial-identifier-character)
       
       (rule :continuing-identifier-character-or-escape
             ((lex-char character) (contains-escapes boolean))
         (production :continuing-identifier-character-or-escape (:continuing-identifier-character) continuing-identifier-character-or-escape-ordinary
           (lex-char ($default-action :continuing-identifier-character))
           (contains-escapes false))
         (production :continuing-identifier-character-or-escape (#\\ :hex-escape) continuing-identifier-character-or-escape-escape
           (lex-char (begin (if (is-continuing-identifier-character (lex-char :hex-escape))
                              (return (lex-char :hex-escape))
                              (throw syntax-error))))
           (contains-escapes true)))
       
       (%charclass :continuing-identifier-character)
       (%print-actions)
       
       (define reserved-words (vector string)
         (vector "abstract" "as" "break" "case" "catch" "class" "const" "continue" "debugger" "default" "delete" "do" "else" "enum"
                 "export" "extends" "false" "final" "finally" "for" "function" "goto" "if" "implements" "import" "in"
                 "instanceof" "interface" "is" "namespace" "native" "new" "null" "package" "private" "protected" "public" "return" "static" "super"
                 "switch" "synchronized" "this" "throw" "throws" "transient" "true" "try" "typeof" "use" "var" "volatile" "while" "with"))
       (define non-reserved-words (vector string)
         (vector "exclude" "get" "include" "named" "set"))
       (define keywords (vector string)
         (append reserved-words non-reserved-words))
       
       (define (member (id string) (list (vector string))) boolean
         (rwhen (empty list)
           (return false))
         (rwhen (= id (nth list 0) string)
           (return true))
         (return (member id (subseq list 1))))
       
       (rule :identifier-or-keyword
             ((lex input-element))
         (production :identifier-or-keyword (:identifier-name) identifier-or-keyword-identifier-name
           (lex (begin
                 (const id string (lex-name :identifier-name))
                 (if (and (member id keywords) (not (contains-escapes :identifier-name)))
                   (return (new keyword id))
                   (return (new identifier id)))))))
       (%print-actions)
       
       (%heading 1 "Punctuators")
       
       (rule :punctuator ((lex token))
         (production :punctuator (#\!) punctuator-not (lex (new punctuator "!")))
         (production :punctuator (#\! #\=) punctuator-not-equal (lex (new punctuator "!=")))
         (production :punctuator (#\! #\= #\=) punctuator-not-identical (lex (new punctuator "!==")))
         (production :punctuator (#\#) punctuator-hash (lex (new punctuator "#")))
         (production :punctuator (#\%) punctuator-modulo (lex (new punctuator "%")))
         (production :punctuator (#\% #\=) punctuator-modulo-equals (lex (new punctuator "%=")))
         (production :punctuator (#\&) punctuator-and (lex (new punctuator "&")))
         (production :punctuator (#\& #\&) punctuator-logical-and (lex (new punctuator "&&")))
         (production :punctuator (#\& #\& #\=) punctuator-logical-and-equals (lex (new punctuator "&&=")))
         (production :punctuator (#\& #\=) punctuator-and-equals (lex (new punctuator "&=")))
         (production :punctuator (#\() punctuator-open-parenthesis (lex (new punctuator "(")))
         (production :punctuator (#\)) punctuator-close-parenthesis (lex (new punctuator ")")))
         (production :punctuator (#\*) punctuator-times (lex (new punctuator "*")))
         (production :punctuator (#\* #\=) punctuator-times-equals (lex (new punctuator "*=")))
         (production :punctuator (#\+) punctuator-plus (lex (new punctuator "+")))
         (production :punctuator (#\+ #\+) punctuator-increment (lex (new punctuator "++")))
         (production :punctuator (#\+ #\=) punctuator-plus-equals (lex (new punctuator "+=")))
         (production :punctuator (#\,) punctuator-comma (lex (new punctuator ",")))
         (production :punctuator (#\-) punctuator-minus (lex (new punctuator "-")))
         (production :punctuator (#\- #\-) punctuator-decrement (lex (new punctuator "--")))
         (production :punctuator (#\- #\=) punctuator-minus-equals (lex (new punctuator "-=")))
         (production :punctuator (#\- #\>) punctuator-arrow (lex (new punctuator "->")))
         (production :punctuator (#\.) punctuator-dot (lex (new punctuator ".")))
         (production :punctuator (#\. #\.) punctuator-double-dot (lex (new punctuator "..")))
         (production :punctuator (#\. #\. #\.) punctuator-triple-dot (lex (new punctuator "...")))
         (production :punctuator (#\:) punctuator-colon (lex (new punctuator ":")))
         (production :punctuator (#\: #\:) punctuator-namespace (lex (new punctuator "::")))
         (production :punctuator (#\;) punctuator-semicolon (lex (new punctuator ";")))
         (production :punctuator (#\<) punctuator-less-than (lex (new punctuator "<")))
         (production :punctuator (#\< #\<) punctuator-left-shift (lex (new punctuator "<<")))
         (production :punctuator (#\< #\< #\=) punctuator-left-shift-equals (lex (new punctuator "<<=")))
         (production :punctuator (#\< #\=) punctuator-less-than-or-equal (lex (new punctuator "<=")))
         (production :punctuator (#\=) punctuator-assignment (lex (new punctuator "=")))
         (production :punctuator (#\= #\=) punctuator-equal (lex (new punctuator "==")))
         (production :punctuator (#\= #\= #\=) punctuator-identical (lex (new punctuator "===")))
         (production :punctuator (#\>) punctuator-greater-than (lex (new punctuator ">")))
         (production :punctuator (#\> #\=) punctuator-greater-than-or-equal (lex (new punctuator ">=")))
         (production :punctuator (#\> #\>) punctuator-right-shift (lex (new punctuator ">>")))
         (production :punctuator (#\> #\> #\=) punctuator-right-shift-equals (lex (new punctuator ">>=")))
         (production :punctuator (#\> #\> #\>) punctuator-logical-right-shift (lex (new punctuator ">>>")))
         (production :punctuator (#\> #\> #\> #\=) punctuator-logical-right-shift-equals (lex (new punctuator ">>>=")))
         (production :punctuator (#\?) punctuator-question (lex (new punctuator "?")))
         (production :punctuator (#\@) punctuator-at (lex (new punctuator "@")))
         (production :punctuator (#\[) punctuator-open-bracket (lex (new punctuator "[")))
         (production :punctuator (#\]) punctuator-close-bracket (lex (new punctuator "]")))
         (production :punctuator (#\^) punctuator-xor (lex (new punctuator "^")))
         (production :punctuator (#\^ #\=) punctuator-xor-equals (lex (new punctuator "^=")))
         (production :punctuator (#\^ #\^) punctuator-logical-xor (lex (new punctuator "^^")))
         (production :punctuator (#\^ #\^ #\=) punctuator-logical-xor-equals (lex (new punctuator "^^=")))
         (production :punctuator (#\{) punctuator-open-brace (lex (new punctuator "{")))
         (production :punctuator (#\|) punctuator-or (lex (new punctuator "|")))
         (production :punctuator (#\| #\=) punctuator-or-equals (lex (new punctuator "|=")))
         (production :punctuator (#\| #\|) punctuator-logical-or (lex (new punctuator "||")))
         (production :punctuator (#\| #\| #\=) punctuator-logical-or-equals (lex (new punctuator "||=")))
         (production :punctuator (#\}) punctuator-close-brace (lex (new punctuator "}")))
         (production :punctuator (#\~) punctuator-complement (lex (new punctuator "~"))))
       
       (rule :division-punctuator ((lex token))
         (production :division-punctuator (#\/ (:- #\/ #\*)) punctuator-divide (lex (new punctuator "/")))
         (production :division-punctuator (#\/ #\=) punctuator-divide-equals (lex (new punctuator "/="))))
       (%print-actions)
       
       (%heading 1 "Numeric Literals")
       
       (rule :numeric-literal ((lex token))
         (production :numeric-literal (:decimal-literal) numeric-literal-decimal
           (lex (new number (real-to-float64 (lex-number :decimal-literal)))))
         (production :numeric-literal (:hex-integer-literal (:- :hex-digit)) numeric-literal-hex
           (lex (new number (real-to-float64 (lex-number :hex-integer-literal))))))
       (%print-actions)
       
       (rule :decimal-literal ((lex-number rational))
         (production :decimal-literal (:mantissa) decimal-literal
           (lex-number (lex-number :mantissa)))
         (production :decimal-literal (:mantissa :letter-e :signed-integer) decimal-literal-exponent
           (lex-number (rat* (lex-number :mantissa) (expt 10 (lex-number :signed-integer))))))
       
       (%charclass :letter-e)
       
       (rule :mantissa ((lex-number rational))
         (production :mantissa (:decimal-integer-literal) mantissa-integer
           (lex-number (lex-number :decimal-integer-literal)))
         (production :mantissa (:decimal-integer-literal #\.) mantissa-integer-dot
           (lex-number (lex-number :decimal-integer-literal)))
         (production :mantissa (:decimal-integer-literal #\. :fraction) mantissa-integer-dot-fraction
           (lex-number (rat+ (lex-number :decimal-integer-literal)
                             (lex-number :fraction))))
         (production :mantissa (#\. :fraction) mantissa-dot-fraction
           (lex-number (lex-number :fraction))))
       
       (rule :decimal-integer-literal ((lex-number integer))
         (production :decimal-integer-literal (#\0) decimal-integer-literal-0
           (lex-number 0))
         (production :decimal-integer-literal (:non-zero-decimal-digits) decimal-integer-literal-nonzero
           (lex-number (lex-number :non-zero-decimal-digits))))
       
       (rule :non-zero-decimal-digits ((lex-number integer))
         (production :non-zero-decimal-digits (:non-zero-digit) non-zero-decimal-digits-first
           (lex-number (decimal-value :non-zero-digit)))
         (production :non-zero-decimal-digits (:non-zero-decimal-digits :a-s-c-i-i-digit) non-zero-decimal-digits-rest
           (lex-number (+ (* 10 (lex-number :non-zero-decimal-digits)) (decimal-value :a-s-c-i-i-digit)))))
       
       (%charclass :non-zero-digit)
       
       (rule :fraction ((lex-number rational))
         (production :fraction (:decimal-digits) fraction-decimal-digits
           (lex-number (rat/ (lex-number :decimal-digits)
                             (expt 10 (n-digits :decimal-digits))))))
       (%print-actions)
       
       (rule :signed-integer ((lex-number integer))
         (production :signed-integer (:decimal-digits) signed-integer-no-sign
           (lex-number (lex-number :decimal-digits)))
         (production :signed-integer (#\+ :decimal-digits) signed-integer-plus
           (lex-number (lex-number :decimal-digits)))
         (production :signed-integer (#\- :decimal-digits) signed-integer-minus
           (lex-number (neg (lex-number :decimal-digits)))))
       (%print-actions)
       
       (rule :decimal-digits
             ((lex-number integer) (n-digits integer))
         (production :decimal-digits (:a-s-c-i-i-digit) decimal-digits-first
           (lex-number (decimal-value :a-s-c-i-i-digit))
           (n-digits 1))
         (production :decimal-digits (:decimal-digits :a-s-c-i-i-digit) decimal-digits-rest
           (lex-number (+ (* 10 (lex-number :decimal-digits)) (decimal-value :a-s-c-i-i-digit)))
           (n-digits (+ (n-digits :decimal-digits) 1))))
       (%print-actions)
       
       (rule :hex-integer-literal ((lex-number integer))
         (production :hex-integer-literal (#\0 :letter-x :hex-digit) hex-integer-literal-first
           (lex-number (hex-value :hex-digit)))
         (production :hex-integer-literal (:hex-integer-literal :hex-digit) hex-integer-literal-rest
           (lex-number (+ (* 16 (lex-number :hex-integer-literal)) (hex-value :hex-digit)))))
       (%charclass :letter-x)
       (%charclass :hex-digit)
       (%print-actions)
       
       (%heading 1 "String Literals")
       
       (grammar-argument :theta single double)
       (rule :string-literal ((lex token))
         (production :string-literal (#\' (:string-chars single) #\') string-literal-single
           (lex (lex-string :string-chars)))
         (production :string-literal (#\" (:string-chars double) #\") string-literal-double
           (lex (lex-string :string-chars))))
       (%print-actions)
       
       (rule (:string-chars :theta) ((lex-string string))
         (production (:string-chars :theta) () string-chars-none
           (lex-string ""))
         (production (:string-chars :theta) ((:string-chars :theta) (:string-char :theta)) string-chars-some
           (lex-string (append (lex-string :string-chars)
                               (vector (lex-char :string-char)))))
         (production (:string-chars :theta) ((:string-chars :theta) :null-escape) string-chars-null-escape
           (lex-string (lex-string :string-chars))))
       
       (rule (:string-char :theta) ((lex-char character))
         (production (:string-char :theta) ((:literal-string-char :theta)) string-char-literal
           (lex-char ($default-action :literal-string-char)))
         (production (:string-char :theta) (#\\ :string-escape) string-char-escape
           (lex-char (lex-char :string-escape))))
       
       (%charclass (:literal-string-char single))
       (%charclass (:literal-string-char double))
       (%print-actions)
       
       (rule :string-escape ((lex-char character))
         (production :string-escape (:control-escape) string-escape-control
           (lex-char (lex-char :control-escape)))
         (production :string-escape (:zero-escape) string-escape-zero
           (lex-char (lex-char :zero-escape)))
         (production :string-escape (:hex-escape) string-escape-hex
           (lex-char (lex-char :hex-escape)))
         (production :string-escape (:identity-escape) string-escape-non-escape
           (lex-char ($default-action :identity-escape))))
       (%charclass :identity-escape)
       (%print-actions)
       
       (rule :control-escape ((lex-char character))
         (production :control-escape (#\b) control-escape-backspace (lex-char #?0008))
         (production :control-escape (#\f) control-escape-form-feed (lex-char #?000C))
         (production :control-escape (#\n) control-escape-new-line (lex-char #?000A))
         (production :control-escape (#\r) control-escape-return (lex-char #?000D))
         (production :control-escape (#\t) control-escape-tab (lex-char #?0009))
         (production :control-escape (#\v) control-escape-vertical-tab (lex-char #?000B)))
       (%print-actions)
       
       (rule :zero-escape ((lex-char character))
         (production :zero-escape (#\0 (:- :a-s-c-i-i-digit)) zero-escape-zero
           (lex-char #?0000)))
       (%print-actions)
       
       (rule :hex-escape ((lex-char character))
         (production :hex-escape (#\x :hex-digit :hex-digit) hex-escape-2
           (lex-char (code-to-character (+ (* 16 (hex-value :hex-digit 1))
                                           (hex-value :hex-digit 2)))))
         (production :hex-escape (#\u :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-4
           (lex-char (code-to-character (+ (+ (+ (* 4096 (hex-value :hex-digit 1))
                                                 (* 256 (hex-value :hex-digit 2)))
                                              (* 16 (hex-value :hex-digit 3)))
                                           (hex-value :hex-digit 4))))))
       
       (%print-actions)
       
       (%heading 1 "Regular Expression Literals")
       
       (rule :reg-exp-literal ((lex token))
         (production :reg-exp-literal (:reg-exp-body :reg-exp-flags) reg-exp-literal
           (lex (new regular-expression (lex-string :reg-exp-body) (lex-string :reg-exp-flags)))))
       
       (rule :reg-exp-flags ((lex-string string))
         (production :reg-exp-flags () reg-exp-flags-none
           (lex-string ""))
         (production :reg-exp-flags (:reg-exp-flags :continuing-identifier-character-or-escape) reg-exp-flags-more
           (lex-string (append (lex-string :reg-exp-flags) (vector (lex-char :continuing-identifier-character-or-escape)))))
         (production :reg-exp-flags (:reg-exp-flags :null-escape) reg-exp-flags-null-escape
           (lex-string (lex-string :reg-exp-flags))))
       
       (rule :reg-exp-body ((lex-string string))
         (production :reg-exp-body (#\/ (:- #\*) :reg-exp-chars #\/) reg-exp-body
           (lex-string (lex-string :reg-exp-chars))))
       
       (rule :reg-exp-chars ((lex-string string))
         (production :reg-exp-chars (:reg-exp-char) reg-exp-chars-one
           (lex-string (lex-string :reg-exp-char)))
         (production :reg-exp-chars (:reg-exp-chars :reg-exp-char) reg-exp-chars-more
           (lex-string (append (lex-string :reg-exp-chars)
                               (lex-string :reg-exp-char)))))
       
       (rule :reg-exp-char ((lex-string string))
         (production :reg-exp-char (:ordinary-reg-exp-char) reg-exp-char-ordinary
           (lex-string (vector ($default-action :ordinary-reg-exp-char))))
         (production :reg-exp-char (#\\ :non-terminator) reg-exp-char-escape
           (lex-string (vector #\\ ($default-action :non-terminator)))))
       
       (%charclass :ordinary-reg-exp-char)
       )))
  
  (defparameter *ll* (world-lexer *lw* 'code-lexer))
  (defparameter *lg* (lexer-grammar *ll*))
  (set-up-lexer-metagrammar *ll*)
  (defparameter *lm* (lexer-metagrammar *ll*)))


#|
(depict-rtf-to-local-file
 "JS20/LexerCharClasses.rtf"
 "JavaScript 2 Lexical Character Classes"
 #'(lambda (rtf-stream)
     (depict-paragraph (rtf-stream :grammar-header)
       (depict rtf-stream "Character Classes"))
     (dolist (charclass (lexer-charclasses *ll*))
       (depict-charclass rtf-stream charclass))
     (depict-paragraph (rtf-stream :grammar-header)
       (depict rtf-stream "Grammar"))
     (depict-grammar rtf-stream *lg*)))

(values
 (depict-rtf-to-local-file
  "JS20/LexerGrammar.rtf"
  "JavaScript 2 Lexical Grammar"
  #'(lambda (rtf-stream)
      (depict-world-commands rtf-stream *lw* :heading-offset 1 :visible-semantics nil)))
 (depict-rtf-to-local-file
  "JS20/LexerSemantics.rtf"
  "JavaScript 2 Lexical Semantics"
  #'(lambda (rtf-stream)
      (depict-world-commands rtf-stream *lw* :heading-offset 1)))
 (depict-html-to-local-file
  "JS20/LexerGrammar.html"
  "JavaScript 2 Lexical Grammar"
  t
  #'(lambda (rtf-stream)
      (depict-world-commands rtf-stream *lw* :heading-offset 1 :visible-semantics nil))
  :external-link-base "notation.html")
 (depict-html-to-local-file
  "JS20/LexerSemantics.html"
  "JavaScript 2 Lexical Semantics"
  t
  #'(lambda (rtf-stream)
      (depict-world-commands rtf-stream *lw* :heading-offset 1))
  :external-link-base "notation.html"))

(with-local-output (s "JS20/LexerGrammar.txt") (print-lexer *ll* s) (print-grammar *lg* s))

(print-illegal-strings m)
|#


#+allegro (clean-grammar *lg*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *lg*))
