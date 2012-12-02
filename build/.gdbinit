# .gdbinit file for debugging Mozilla

# You may need to put an 'add-auto-load-safe-path' command in your
# $HOME/.gdbinit file to get GDB to trust this file. If your builds are
# generally in $HOME/moz, then you can say:
#
#  add-auto-load-safe-path ~/moz

# Don't stop for the SIG32/33/etc signals that Flash produces
handle SIG32 noprint nostop pass
handle SIG33 noprint nostop pass
handle SIGPIPE noprint nostop pass

# Show the concrete types behind nsIFoo
set print object on

# run when using the auto-solib-add trick
def prun
        tbreak main
        run
	set auto-solib-add 0
        cont
end

# run -mail, when using the auto-solib-add trick
def pmail
        tbreak main
        run -mail
	set auto-solib-add 0
        cont
end

# Define a "pu" command to display PRUnichar * strings (100 chars max)
# Also allows an optional argument for how many chars to print as long as
# it's less than 100.
def pu
  set $uni = $arg0
  if $argc == 2
    set $limit = $arg1
    if $limit > 100
      set $limit = 100
    end
  else
    set $limit = 100
  end
  # scratch array with space for 100 chars plus null terminator.  Make
  # sure to not use ' ' as the char so this copy/pastes well.
  set $scratch = "____________________________________________________________________________________________________"
  set $i = 0
  set $scratch_idx = 0
  while (*$uni && $i++ < $limit)
    if (*$uni < 0x80)
      set $scratch[$scratch_idx++] = *(char*)$uni++
    else
      if ($scratch_idx > 0)
	set $scratch[$scratch_idx] = '\0'
	print $scratch
	set $scratch_idx = 0
      end
      print /x *(short*)$uni++
    end
  end
  if ($scratch_idx > 0)
    set $scratch[$scratch_idx] = '\0'
    print $scratch
  end
end

# Define a "ps" command to display subclasses of nsAC?String.  Note that
# this assumes strings as of Gecko 1.9 (well, and probably a few
# releases before that as well); going back far enough will get you
# to string classes that this function doesn't work for.
def ps
  set $str = $arg0
  if (sizeof(*$str.mData) == 1 && ($str.mFlags & 1) != 0)
    print $str.mData
  else
    pu $str.mData $str.mLength
  end
end

# Define a "pa" command to display the string value for an nsIAtom
def pa
  set $atom = $arg0
  if (sizeof(*((&*$atom)->mString)) == 2)
    pu (&*$atom)->mString
  end
end

# define a "pxul" command to display the type of a XUL element from
# an nsXULDocument* pointer.
def pxul
  set $p = $arg0
  print $p->mNodeInfo.mRawPtr->mInner.mName->mStaticAtom->mString
end

# define a "prefcnt" command to display the refcount of an XPCOM obj
def prefcnt
  set $p = $arg0
  print ((nsPurpleBufferEntry*)$p->mRefCnt.mTagged)->mRefCnt
end

# define a "ptag" command to display the tag name of a content node
def ptag
  set $p = $arg0
  pa $p->mNodeInfo.mRawPtr->mInner.mName
end

##
## nsTArray
##
define ptarray
        if $argc == 0
                help ptarray
        else
                set $size = $arg0.mHdr->mLength
                set $capacity = $arg0.mHdr->mCapacity
                set $size_max = $size - 1
                set $elts = $arg0.Elements()
        end
        if $argc == 1
                set $i = 0
                while $i < $size
                        printf "elem[%u]: ", $i
                        p *($elts + $i)
                        set $i++
                end
        end
        if $argc == 2
                set $idx = $arg1
                if $idx < 0 || $idx > $size_max
                        printf "idx1, idx2 are not in acceptable range: [0..%u].\n", $size_max
                else
                        printf "elem[%u]: ", $idx
                        p *($elts + $idx)
                end
        end
        if $argc == 3
          set $start_idx = $arg1
          set $stop_idx = $arg2
          if $start_idx > $stop_idx
            set $tmp_idx = $start_idx
            set $start_idx = $stop_idx
            set $stop_idx = $tmp_idx
          end
          if $start_idx < 0 || $stop_idx < 0 || $start_idx > $size_max || $stop_idx > $size_max
            printf "idx1, idx2 are not in acceptable range: [0..%u].\n", $size_max
          else
            set $i = $start_idx
                while $i <= $stop_idx
                        printf "elem[%u]: ", $i
                        p *($elts + $i)
                        set $i++
                end
          end
        end
        if $argc > 0
                printf "nsTArray length = %u\n", $size
                printf "nsTArray capacity = %u\n", $capacity
                printf "Element "
                whatis *$elts
        end
end

document ptarray
        Prints nsTArray information.
        Syntax: ptarray   
        Note: idx, idx1 and idx2 must be in acceptable range [0...size()-1].
        Examples:
        ptarray a - Prints tarray content, size, capacity and T typedef
        ptarray a 0 - Prints element[idx] from tarray
        ptarray a 1 2 - Prints elements in range [idx1..idx2] from tarray
end

def js
  call DumpJSStack()
end

def ft
  call nsFrame::DumpFrameTree($arg0)
end
