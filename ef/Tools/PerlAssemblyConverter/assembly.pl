#/*
# Christian Bennett
#
# Purpose:
# For use in a Make system when generating the at&t assembly syntax file (for gnu software) from some the 
# Intel standard assembly syntax file.
#
#
# Expandability:  
# This program is ready to be hooked into a make system.  The design for this script
# is based on the assumption there is just an assembly file with no distinct header or 
# footer information.  This was not designed to parse through a C++ file and find the inlined
# assembly.  Quite simply it takes an Intel asm instruction and outputs the AT&T version.  If
# there is a line it does not recognize, it outputs it as is.  
# So if there is distinct headers and footers per compiler you may have to modify the linux 
# output slightly, or modify this code slightly.
#
# Use:  
# The IO in this file is done through standard in and standard out.  READ: cat the input  
# and redirect the output to a file.  Or do whatever means necessary.  So for testing I did something
# like:  cat sampleInput.asm perl assembly.pl > linuxTranslation.pl
#
#
# KEY: in comments: ws = whitespcae, pws = possible white space (read: there may be ws here).


# Here is a hash for identifying registers.  Querying the hash will return TRUE if, the query contains a
# register.  Else will return null.  If there are additional registers that may be used, add them in a 
# similar fasion.
#*/

%registers = (
	"eax" => 1,
	"ebx" => 1,
	"ecx" => 1,
	"edx" => 1,
	"esi" => 1,
	"edi" => 1,
	"eip" => 1,
	"esp" => 1,
	"ebp" => 1,
	"efl" => 1,
	);



LINE: while ($line = <>) {
	
	if($line eq "\n"){		#goto next line if endline
		print "\n";
		next LINE; 
	}



#/*----------------Interpretation of instructions--------------
### The Different Cases Handled
#
#	Each case takes a string.  It then parses the string and grabs arguments and parameters.
#	It then formats the instruction and args for AT&T assembly syntax.
#
#	Formating preserves c++ style comments.
#
#	1) instruction = insn reg, int
#	   This case grabs the three words, and makes sure the second is a reg, and the third a integer
#
#	2) insn reg, reg2
#	   This case grabs the three words, and check if the 2nd and 3rd parameters are valid registers.
#
#	3) insn reg, [reg + offet] (where offset is an integer)
#      This case grabs 5 items: inst, reg, reg, sign, offset.
#
#	4) insn int
#      This case grabs the instruction, then grabs an int.
#
#	5) insn location (e.g. call FOO)
#	   insn reg		
#
#	6) inverse of case 3: insn [reg + offset], reg 
#	
#   7) insn reg, [reg2 + FOO * 4]  //note FOO may be a register.
#
#	8) insn [reg + FOO*4], reg2
#
#	9) insn reg, [reg2]
#	
#  10) insn [reg], reg2
#
#  11) insn reg, [reg2 + reg3]
#
#  12) insn [reg + reg2], reg3
#
#  13) insn reg, [reg2 + reg3 + offset]  
#
#  14) insn [reg + reg2 + offset], reg4
#  
#  15) insn reg, [reg2 + reg3*offset + someintOffset]
#
#  16) insn [reg1 + reg2*offset + someintOffset], reg3
#
#
#
#*/

	if($line =~ m@(^//.*)@){						#if a line is a comment, 
		print $1, "\n";								#send to the output	stream.
		next LINE;
	}	


#/*   CASE 1 -- insn reg, int	 --> insnl %reg, int
#	This statment is a regexp looking for: a word, whitespace, alphaNum chars, possible space
#   an integer (digit), possible whitespace, possible comment.  
#   It then to makes sure the second word it absorbed was a register by using the hash feature.
#   If entered, 'next' gotos the next iteration of the while loop.
#*/

	if($line =~ m@(\w+)\s+([a-zA-Z]+),\s*(\d+)\s*(/*.*)@ && $registers{$2}){ 		 
		print $1, "l     %", $2, ", ", $3;
		print "              ", $4, "\n";
		next LINE;
	} 



#/*   CASE 2 -- insn reg, reg2    -->  insnl %reg2, %reg
#	This statment is a regexp looking for: a word, whitespace, a word, possible space
#   alphanum chars, possible whitespace, possible comments.  
#   It then to makes sure the second and third words absorbed were registers by 
#   using the hash feature.
#   If entered, 'next' gotos the next iteration of the while loop.
#*/


	if(($line =~ m@(\w+)\s+(\w+),\s*([a-zA-Z]+)\s*(/*.*)@)							 
		         && $registers{$2} && $registers{$3}){									
		print $1, "l       %", $3, ", %", $2;
		print "           ", $4, "\n";			
		next LINE;
	}



#/*   CASE 3-- insn reg, [reg2+offset]   --> insnl offset(%reg2), %reg
#	This statment is a regexp looking for: a word, whitespace, alphanum chars, possible space
#   [ char, alphanum chars, possible whitespace, + or -, possible whitespace, ] char,
#   possible comments. It then to makes sure the second and third words absorbed were 
#   actually registers by using the hash feature.
#   If entered, 'next' gotos the next iteration of the while loop.
#*/
  
	
	if($line =~ m@(\w+)\s+([a-zA-Z]+),\s*\[([a-zA-Z]+)\s*([\+\-])\s*(\d+)\s*]\s*(/*.*)@	 
										&& $registers{$2} && $registers{$3}){			

		if($4 eq "\+"){
			print $1, "l       ", $5, "(%", $3, "), %", $2;		#if +
		}
		else{
			print $1, "l      -", $5, "(%", $3, "), %", $2;		#if -
		}

		print "        ", $6, "\n";
		next LINE;
	}
	
#/*   CASE 4 -- insn int   -->  insnl $int, or ret int
#	This statment is a regexp looking for: a word, whitespace, digits, possible space
#   possible comments. It then to makes sure the second word absorbed was
#   actually a register by using the hash feature.
#	Also makes sure there are no '['s in the string.
#   If entered, 'next' gotos the next iteration of the while loop.
#*/
  
	if($line =~ m@(\w+)\s+(\d+)\s*(/*.*)@ && !$registers{$2} && !($line =~ /\[/)){		 
		$temp = $1;
		if($1 eq "ret"){
			print $temp, "        \$", $2;
		}
		else{
			print $temp, "l      \$", $2;
		}
		print "                  ", $3, "\n";
		next LINE;
	}
	

#/*   CASE 5  insn LOCATION or insn reg	--> either insn *%reg  or insnl %LOCATION
#	This statment is a regexp looking for: a word, whitespace, alphanum chars, possible space
#   possible comments. It tests to exclude strings with '[' in them.
#   If entered, 'next' gotos the next iteration of the while loop.
#*/

  if($line =~ m@(\w+)\s+([a-zA-Z]+)\s*(/*.*)@ && !($line =~ /\[/)){	    			
		$temp = $1;
		$temp2 = $2;
		
		if((($registers{$2}) && (($temp eq "call") || ($temp eq "jmp")))){
				print $temp, "       *\%", $temp2;
		}	
		elsif($registers{$2}){
				print $temp, "l      \%", $temp2;
			
		}
		else{
			print $, "     ", $2;
		}
		
		print "                 ", $3, "\n";
		next LINE;
	}

#/*   CASE 6 insn [reg + offset], reg2  -- >  insnl %reg2, offset(%reg)
#	This statment is a regexp looking for: a word, whitespace, alphanum chars, possible space
#   + or - char, possible whitespace, possible whitespace, digits, ws,], ws, alphanum chars, ws,
#   possible comments. It then to makes sure the second and fifth words absorbed were 
#   actually registers by using the hash feature.
#   If entered, 'next' gotos the next iteration of the while loop.
#*/

  if($line =~ m@(\w+)\s+\[([a-zA-Z]+)\s*([+-])\s*(\d+)\s*\]\s*,\s*([a-zA-Z]+)\s*(/*.*)@ &&		 
		$registers{$2} && $registers{$5}){									
																						

		if($3 eq "\+"){
			print $1, "l       ", $5, ",    ", $4, "(", $2, ")";		#if +
		}
		else{
			print $1, "l       ", $5, ",   -", $4, "(", $2, ")";		#if -
		}

		print "       ", $6, "\n";
		next LINE;
	}


# CASE 7 : insn reg, [reg2 + FOO*int] --> insnl %reg2(FOO*int), %reg
#		   E.G. : add eax, [ecx + FOO*4] --> addl %ecx(FOO*4),  %eax

	if($line =~ m@(\w+)\s+([a-zA-Z]+),\s*\[([a-zA-Z]+)\s*([\+\-])\s*([a-zA-Z]+)\*(\d+)\s*]\s*(/*.*)@
										&& $registers{$2} && $registers{$3}){			
		
		if($registers{$5}){														#if FOO is a register
			if($4 eq "\+"){
				print $1, "l    %", $3, "(%", $5, "*", $6, "),    %", $2;		#if +
			}
			else{
				print $1, "l    %", $3, "(-%", $5, "*", $6, "),    %", $2;		#if -
			}
		}
		else{																	
			if($4 eq "\+"){
				print $1, "l    %", $3, "(", $5, "*", $6, "),    %", $2;		
			}
			else{
				print $1, "l    %", $3, "(-", $5, "*", $6, "),    %", $2;		
			}
		}
		print "        ", $7, "\n";
		next LINE;
	}



# case 8: insn [reg1 + FOO*int], reg2  -->  insnl %reg2, %reg1(%FOO*int) if foo is a reg
#   								   -->  insnl %reg2, %reg1(FOO*int)  if foo is not a register
# This code first checks to see if foo is a register.  If foo is a register, it switches on whether 
# or not there was a plus or minus symbol used.  The rest is formatting to make the case mentioned above
# have the right output.
#
	if($line =~ m@(\w+)\s+\[([a-zA-Z]+)\s*([+-])\s*(\w+)\s*\*\s*(\d+)\s*\]\s*,\s*(\w+.*)\s*(/*.*)@	 
										&& $registers{$2} && $registers{$5}){			
		
		if($registers{$5}){														#if FOO is a register
			if($4 eq "\+"){
				print $1, "l    %", $6, ", %", $2, "(%", $4, "*", $5, ")";		#if +
			}
			else{
				print $1, "l    %", $6, ", %", $2, "(-%", $4, "*", $5, ")";		#if -
			}
		}
		else{																	#if FOO is not a register
			if($4 eq "\+"){
				print $1, "l    %", $6, ", %", $2, "(", $4, "*", $5, ")";		
			}
			else{
				print $1, "l    %", $6, ", %", $2, "(-", $4, "*", $5, ")";		
			}
		}
		print "        ", $7, "\n";
		next LINE;
	}

# case 9: insn reg, [reg2] --> insn (%reg2), %reg
# This code looks for and write the input and output described above.  
# The only error checking is the identical matching of the input (with ws as a variant), and
# to make sure the args are valid registers.
# The rest is formatting to make the case mentioned above have the right output.

	if($line =~ m@(\w+)\s+(\w+),\s*\[(\w+)\]\s*(/*.*)@ && $registers{$2} && $registers{$3}){
		
		  print $1, "l    (%", $3, "), %", $2;
		  print "         ", $4, "\n";
		  next LINE;

	}


# Case 10: insn [reg], reg2 --> insn %reg2, (%reg)
# Just the reverse of the previous case.  See case 9.
	if($line =~ m@(\w+)\s+\[(\w+)\],\s*(\w+)\s*(/*.*)@ && $registers{$2} && $registers{$3}){             #*/
	
		  print $1, "l	 %", $3, ", (%", $2, ")";
		  print "         ", $4, "\n";
		  next LINE;
	}
	


# case 11: insn reg, [reg2 + reg3]   --> insnl (%reg2, %reg3), %reg
#
# This looks for the input matching the description above.  If found it then formats it match the output described.
# Error checking is in the formatting and making sure appropriate args are valid registers.
# If entered, goto LINE to iterate through the next line.

	
	if($line =~ m@(\w+)\s+(\w+),\s*\[\s*([a-zA-Z]+)\s*([/+/-])\s*([a-zA-Z]+)\s*\]\s*(/*.*)@ && $registers{$2}  #*/
		&& $registers{$3} && $registers{$5}){
		
			if($4 eq "\+"){
				print $1, "l   (%", $3, ",%", $5, "), ", $2;
			}
			else{
								# does subtraction work??
			}

		print "         ", $6, "\n";
		next LINE;
	 }


# case 12: insn [reg + reg2], reg3  (reverse of case 11) --> insnl  %reg3, (%reg, %reg2)
# See case 11 for details.

	if($line =~ m@(\w+)\s+\[\s*([a-zA-Z]+)\s*([+-])\s*([a-zA-Z]+)\s*\],\s*([a-zA-Z]+)\s*(/*.*)@){
		
		if($3 eq "\+"){

			print $1, "l    %", $5, ", (%", $2, ",%", $4, ")";
		}
		print "          ", $6, "\n";
		next LINE;
	}

# case 13: insn reg, [reg2 + reg3 + int]    -->   insnl (%reg2, %reg3, int), %reg
# The following code takes the above
#
#
	
	if($line =~ m@(\w+)\s+(\w+),\s*\[\s*([a-zA-Z]+)\s*([/+/-])\s*([a-zA-Z]+)\s*\+\s*(\d+)\s*\]\s*(/*.*)@        #*/
	&& $registers{$2} && $registers{$3} && $registers{$5}){
		
			if($4 eq "\+"){
				print $1, "l   (%", $3, ",%", $5, ",", $6, "), ", $2;
			}
			else{
								# does subtraction work??
			}

		print "         ", $7, "\n";
		next LINE;
	 }


#  case 14: insn [reg + reg2 + int], reg3  (reverse of case 13) -->  insnl %reg3, (%reg, %reg2, int)
#
#    The regular expression parses for (in this order): word, ws, '[', pws, Chars, pws, + or -, 
#    pws, chars, pws, +, pws, int, pws, ']', ',',pws, chars, pws, possible comments 
#
#	 Grabs and puts items in temp vars $1-$7 respectively: insn, reg, sign, reg, int, reg, comment
#

	if($line =~ m@(\w+)\s+\[\s*([a-zA-Z]+)\s*([+-])\s*([a-zA-Z]+)\s*\+\s*(\d*)\s*\],\s*([a-zA-Z]+)\s*(/*.*)@){
		
		if($3 eq "\+"){

			print $1, "l    %", $6, ", (%", $2, ",%", $4, ",", $5, ")";
		}
		print "          ", $7, "\n";
		next LINE;
	}


#  case 15: insn reg, [reg2 + reg3*4 + int] --> insn   (%reg2,%reg3*4,int), reg
#
#	 The regular expression parses for (in this order): word, ws, word, ',', pws, '['
#    pws, chars, pws, + or -, pws, chars, pws, '*', int, pws, +, pws, int, pws, ']', pws,
#	 possible comments.
#
#	 Grabs and puts items temp vars $1-$8 respectively: insn, reg, reg2, reg3, int, + or -, int, comment
#	

	if($line =~ m@(\w+)\s+(\w+),\s*\[\s*([a-zA-Z]+)\s*([/+/-])\s*([a-zA-Z]+)\s*\*\s*(\d+)\s*\+\s*(\d+)\s*\]\s*(/*.*)@ 
	&& $registers{$2} && $registers{$3} && $registers{$5}){
					

			if($4 eq "\+"){
				print $1, "l   ", $6, "(%", $3, ",%", $5, ",", $7, "), %", $2;
			}
			else{
								# does subtraction work??
			}

		print "         ", $8, "\n";
		next LINE;
	 }


#  case 16: insn [reg + reg2*4 + int], reg3  (reverse of case 12) --> insn    %reg3, (%reg,%reg2*4,int)
#		  The regular expression parses for (in this order): word, ws, '[', possible ws, chars, poss ws,
#		  + or -, possible ws, chars, pws, '*', int, pws, '+', pws, int, pws, ']', pws, chars, pws 	 		
#		  then looks for possible comments.
#
#		  Grabs and puts items temp vars $1-$8 respectively: insn, reg, reg2, int, + or -, int, reg3, comment
#
#

		if($line =~ m@(\w+)\s+\[\s*([a-zA-Z]+)\s*([+-])\s*([a-zA-Z]+)\s*\*\s*(\d*)\s*\+\s*(\d*)\s*\],\s*([a-zA-Z]+)\s*(/*.*)@
				&& $registers{$2} && $registers{$4} && $registers{$7}){
		

			if($3 eq "\+"){

				print $1, "l    %", $7, ", ", $5, "(%", $2, ",%", $4, ",", $6, ")";
			}
			print "          ", $8, "\n";
			next LINE;

		}

	
# ELSE just output the line as is...
		
		if($line =~ /\s*(.+)/){
			print $1, "\n";
		}
}


