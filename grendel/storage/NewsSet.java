/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Jamie Zawinski <jwz@netscape.com>, 10 May 1995.
 * Ported to Java, 4 Sep 1997.
 */

package grendel.storage;

import calypso.util.ByteBuf;
import calypso.util.Assert;

/** This class represents a set of integers.  It uses a highly compressed
    encoding, taking advantage of the assumption that many of the integers
    are consecutive.  This is intended for representing lines from the
    .newsrc file, which have lists of message-numbers lists like
   <PRE>
       1-29627,29635,29658,32861-32863</PRE>
   <P>so the data has these properties:
   <P><UL>
   <LI> strictly increasing;
   <LI> large subsequences of monotonically increasing ranges;
   <LI> gaps in the set are usually small, but not always;
   <LI> consecutive ranges tend to be large.
   </UL>
  */

/* The biggest win is to run-length encode the data, storing ranges as two
   numbers (start+length or start,end). We could also store each number as a
   delta from the previous number for further compression, but that gets kind
   of tricky, since there are no guarentees about the sizes of the gaps, and
   we'd have to store variable-length words.

   Current data format:

   DATA := SIZE [ CHUNK ]*
   CHUNK := [ RANGE | VALUE ]
   RANGE := -LENGTH START
   START := VALUE
   LENGTH := int32
   VALUE := a literal positive integer, for now
            it could also be an offset from the previous value.

   LENGTH could also perhaps be a less-than-32-bit quantity,
   at least most of the time.

   Lengths of CHUNKs are stored negative to distinguish the beginning of
   a chunk from a literal: negative means two-word sequence, positive
   means one-word sequence.

   0 represents a literal 0, but should not occur, and should never occur
   except in the first position.

   A length of -1 won't occur either, except temporarily - a sequence of
   two elements is represented as two literals, since they take up the same
   space.

   Another optimization we make is to notice that we typically ask the
   question ``is N a member of the set'' for increasing values of N. So the
   set holds a cache of the last value asked for, and can simply resume the
   search from there.
 */

public class NewsSet {

  private long cached_value = -1;
  private int cached_value_index = 0;

  private int data_length = 0;
  private int data_size = 0;
  private long data[] = null;

  public NewsSet() {
    data_size = 10;
    data = new long[data_size];
  }

  public NewsSet(String chars) {
    this(chars.getBytes(), 0, chars.length());
  }

  public NewsSet(ByteBuf chars) {
    this(chars.toBytes(), 0, chars.length());
  }

  public NewsSet(byte chars[], int start, int end) {
    data_size = 10;
    data = new long[data_size];
    if (end == 0) return;

    int in = start;
    while (in < end) {
      long from = 0;
      long to;

      if (data_length > data_size - 4)
        // out of room!
        grow();

      while (in < end && chars[in] <= ' ')
        in++;

      if (in < end && (chars[in] < '0' || chars[in] > '9'))
        break;                  // illegal character

      while (in < end &&
             chars[in] >= '0' && chars[in] <= '9')
        from = (from * 10) + (chars[in++] - '0');

      while (in < end && chars[in] <= ' ')
        in++;

      if (in < end && chars[in] != '-') {
        to = from;
      } else {
        to = 0;
        in++;
        while (in < end && chars[in] >= '0' && chars[in] <= '9')
          to = (to * 10) + (chars[in++] - '0');
        while (in < end && chars[in] <= ' ')
          in++;
      }

      if (to < from) to = from; /* illegal */

      // This is a total kludge - if the newsrc file specifies a range 1-x as
      // being read, we internally pretend that article 0 is read as well.
      // (But if only 2-x are read, then 0 is not read.)  This is needed
      // because some servers think that article 0 is an article (I think)
      // but some news readers (including Netscape 1.1) choke if the .newsrc
      // file has lines beginning with 0...
      //
      if (from == 1) from = 0;

      if (to == from) {
        // Write it as a literal
        data[data_length++] = from;
      } else {
        // Write it as a range.
        data[data_length++] = -(to - from);
        data[data_length++] = from;
      }

      while (in < end && (chars[in] == ',' || chars[in] <= ' '))
        in++;
    }
  }

  private void grow() {
    int new_size = data_size * 2;
    long new_data[] = new long[new_size];
    System.arraycopy(data, 0, new_data, 0, data_length);
    data_size = new_size;
    data = new_data;
  }


  /** Returns the lowest non-member of the set greater than 0.
      Note that this never returns -1, since a NewsSet can't
      hold the set of positive integers.
   */
  public long firstNonMember() {
    if (data_length <= 0) {
      return 1;
    } else if (data[0] < 0 && data[1] != 1 && data[1] != 0) {
      // first range not equal to 0 or 1, always return 1
      return 1;
    } else if (data[0] < 0) {
      // it's a range
      // If there is a range [N-M] we can presume that M+1 is not in the set.
      return (data[1] - data[0] + 1);
    } else {
      // it's a literal
      if (data[0] == 1) {
        // handle "1,..."
        if (data_length > 1 && data[1] == 2) {
          // This is "1,2,M-N,..." or "1,2,M,..."  where M >= 4.  Note
          // that M will never be 3, because in that case we would have
          // started with a range: "1-3,..." */
          return 3;
        } else {
          return 2;       // handle "1,M-N,.." or "1,M,..." where M >= 3.
        }
      }
      else if (data[0] == 0) {
        // handle "0,..."
        if (data_length > 1 && data[1] == 1) {
          // this is 0,1, (see above)
          return 2;
        }
        else {
          return 1;
        }

      } else {
        // handle "M,..." where M >= 2.
        return 1;
      }
    }
  }

  /** Returns the smallest element of the set.
      Returns -1 if the set is empty.
    */
  public long min() {
    if (data_length > 1) {
      long first = data[0];
      if (first < 0) {  // is range at start?
        long second = data[1];
        return (second);
      } else {          // no, so first number must be first member
        return data[0];
      }
    } else if (data_length == 1)
      return data[0];   // must be only 1 read.
    else
      return -1;
  }

  /** Returns the largest element of the set.
      Returns -1 if the set is empty.
    */
  public long max() {
    if (data_length > 1) {
      long nextToLast = data[data_length - 2];
      if (nextToLast < 0) {     // is range at end?
        long last = data[data_length - 1];
        return (-nextToLast + last - 1);
      } else {
        // no, so last number must be last member
        return data[data_length - 1];
      }
    } else if (data_length == 1)
      return data[0];   // must be only 1 read.
    else
      return -1;
  }

  /** Returns whether the number is a member of the set. */
   public boolean member(long number) {

     boolean value = false;

     int i = 0;

     // If there is a value cached, and that value is smaller than the
     // value we're looking for, skip forward that far.
     if (cached_value > 0 && cached_value < number)
       i = cached_value_index;

     while (i < data_length) {
       if (data[i] < 0) {
         // it's a range
         long from = data[i+1];
         long to = from + (-(data[i]));

         if (from > number) {
           // This range begins after the number - we've passed it.
           value = false;
           break;

         } else if (to >= number) {
           // In range.
           value = true;
           break;
         } else {
           i += 2;
         }

       } else {
         // it's a literal
         if (data[i] == number) {
           // bang
           value = true;
           break;
         } else if (data[i] > number) {
           // This literal is after the number - we've passed it.
           value = false;
           break;
         } else {
           i++;
         }
       }
     }

     // Store the position of this chunk for next time.
     cached_value = number;
     cached_value_index = i;
     return value;
   }

  /** Cause the number to be a member of the set.
      Returns false if the number was already a member of the set,
      true otherwise.
    */
   public boolean insert(long number) {

     int i = 0;

     if (number < 0)
       throw new Error("number must be non-negative");

     // We're going to modify the set, so invalidate the cache.
     cached_value = -1;

     while (i < data_length) {
       if (data[i] < 0) {
         // it's a range
         long from = data[i+1];
         long to = from + (-(data[i]));

         if (from <= number && to >= number) {
           // This number is already present - we don't need to do anything.
           return false;
         }

         if (to > number) {
           // We have found the point before which the new number should be
           // inserted.
           break;
         }

         i += 2;

       } else {
         // it's a literal
         if (data[i] == number) {
           // This number is already present - we don't need to do anything.
           return false;
         }

         if (data[i] > number) {
           // We have found the point before which the new number should be
           // inserted.
           break;
         }
         i++;
       }
     }

     // At this point, `i' points to a position in the set which represents
     // a value greater than `number'; or it is at `data_length'. In the
     // interest of avoiding massive duplication of code, simply insert a
     // literal here and then run the optimizer.
     //

     if (data_size <= data_length + 1)
       grow();

     if (i >= data_length) {
       // at the end: Add a literal to the end.
       data[data_length++] = number;

     } else {
       // need to insert (or edit) in the middle.
       for (int j = data_length; j > i; j--)            // open up a space;
         data[j] = data[j-1];
       data[i] = number;                                // insert.
       data_length++;
     }

     optimize();
     markDirty();
     return true;
   }

  /** Cause the number to not be a member of the set.
      Returns true if the number had been a member of the set,
      false otherwise.
    */
   public boolean delete(long number) {

     if (number < 0)
       throw new Error("number must be non-negative");

     // We're going to modify the set, so invalidate the cache.
     cached_value = -1;

     int i = 0;
     while (i < data_length) {
       int mid = i;

       if (data[i] < 0) {
         // it's a range
         long from = data[i+1];
         long to = from + (-(data[i]));

         if (number < from || number > to) {
           // Not this range
           i += 2;
           continue;
         }

         if (to == from + 1) {
           // If this is a range [N - N+1] and we are removing M
           // (which must be either N or N+1) replace it with a
           // literal. This reduces the length by 1.
           //
           data[mid] = (number == from ? to : from);
           while (++mid < data_length) {
             data[mid] = data[mid+1];
           }
           data_length--;
           optimize();
           markDirty();
           return true;

         } else if (to == from + 2) {
           // If this is a range [N - N+2] and we are removing M,
           // replace it with the literals L,M (that is, either
           // (N, N+1), (N, N+2), or (N+1, N+2). The overall
           // length remains the same.
           //
           data[mid] = from;
           data[mid+1] = to;
           if (from == number) {
             data[mid] = from+1;
           } else if (to == number) {
             data[mid+1] = to-1;
           }
           optimize();
           markDirty();
           return true;

         } else if (from == number) {
           // This number is at the beginning of a long range (meaning a
           // range which will still be long enough to remain a range.)
           // Increase start and reduce length of the range.
           //
           data[mid]++;
           data[mid+1]++;
           optimize();
           markDirty();
           return true;

         } else if (to == number) {
           // This number is at the end of a long range (meaning a range
           // which will still be long enough to remain a range.)
           // Just decrease the length of the range.
           //
           data[mid]++;
           optimize();
           markDirty();
           return true;

         } else {
           // The number being deleted is in the middle of a range which
           // must be split. This increases overall length by 2.
           //
           if (data_size - data_length <= 2)
             grow();

           for (int j = data_length + 2; j > mid + 2; j--) {
             data[j] = data[j-2];
           }

           data[mid] = (- (number - from - 1));
           data[mid+1] = from;
           data[mid+2] = (- (to - number - 1));
           data[mid+3] = number + 1;
           data_length += 2;

           // Oops, if we've ended up with a range with a 0 length,
           // which is illegal, convert it to a literal, which reduces
           // the overall length by 1.
           //
           if (data[mid] == 0) {
             // first range
             data[mid] = data[mid+1];
             for (int j = mid + 1; j < data_length; j++) {
               data[j] = data[j+1];
             }
             data_length--;
           }

           if (data[mid+2] == 0) {
             // second range
             data[mid+2] = data[mid+3];
             for (int j = mid + 3; j < data_length; j++) {
               data[j] = data[j+1];
             }
             data_length--;
           }
           optimize();
           markDirty();
           return true;
         }

       } else {
         // it's a literal
         if (data[i] != number) {
           // Not this literal
           i++;
           continue;
         }

         // Excise this literal.
         data_length--;
         while (mid < data_length) {
           data[mid] = data[mid+1];
           mid++;
         }
         optimize();
         markDirty();
         return true;
       }
     }

     // It wasn't here at all.
     return false;
   }


  private final int emit_range(long out[], int i, long a, long b) {
    if (a == b) {
      out[i++] = a;
    } else {
      Assert.Assertion(a < b && a >= 0);
      if (a >= b || a < 0) throw new Error("emitting out of order");
      out[i++] = -(b - a);
      out[i++] = a;
    }
    return i;
  }


  /** Cause the numbers in the range [start, end) to be members of the set.
      Returns false if all of the numbers were already members of the set,
      true otherwise.
    */
   public boolean insert(long start, long end) {

     if (start >= end)
       throw new Error("start must be < end");

     end--;   // the code here operates on [start, end], not [start, end).

     if (start == end)
       return insert(start);

     boolean didit = false;

     // We're going to modify the set, so invalidate the cache.
     cached_value = -1;

     int new_data_length = data_length + 2;
     long new_data[] = new long[new_data_length];

     int in = 0;
     int out = 0;

     while (in < data_length) {
       long a;
       long b;
       // Set [a,b] to be this range.
       if (data[in] < 0) {
         b = - data[in++];
         a = data[in++];
         b += a;
       } else {
         a = b = data[in++];
       }

       if (a <= start && b >= end) {
         // We already have the entire range marked.
         return false;
       }

       if (start > b + 1) {
         // No overlap yet.
         out = emit_range(new_data, out, a, b);
       } else if (end < a - 1) {
         // No overlap, and we passed it.
         out = emit_range(new_data, out, start, end);
         out = emit_range(new_data, out, a, b);
         didit = true;
         break;
       } else {
         // The ranges overlap.  Suck this range into our new range, and
         // keep looking for other ranges that might overlap.
         start = (start < a ? start : a);
         end = (end > b ? end : b);
       }
     }
     if (!didit)
       out = emit_range(new_data, out, start, end);

     while (in < data_length) {
       new_data[out++] = data[in++];
     }

     data = new_data;
     data_length = out;
     data_size = data_length;
     markDirty();
     return true;
   }


  /** Cause the numbers in the range [start, end) to not be members of the set.
      Returns true if any of the numbers had been members of the set, false
      otherwise.
    */
   public boolean delete(long start, long end) {

     if (start >= end)
       throw new Error("start must be < end");

     boolean any = false;
     // #### OPTIMIZE THIS!
     // #### ADD A SELF-TEST!
     while (start < end) {
       boolean b = delete(start++);
       if (b) any = true;
     }
     return any;
  }

  /** Returns the number of elements in the range [start, end)
      which are <I>not</I> members of the set.
    */
   public long countMissingInRange(long range_start, long range_end) {

     if (range_start >= range_end)
       throw new Error("range_start must be < range_end");

     // #### ADD A SELF-TEST!

     range_end--;   // the code operates on [start, end], not [start, end).

     long count = range_end - range_start + 1;

     int i = 0;
     while (i < data_length) {
       if (data[i] < 0) {
         // it's a range
         long from = data[i+1];
         long to = from + (-(data[i]));
         if (from < range_start) from = range_start;
         if (to > range_end) to = range_end;

         if (to >= from)
           count -= (to - from + 1);

         i += 2;
       } else {
         // it's a literal
         if (data[i] >= range_start && data[i]<= range_end)
           count--;
         i++;
       }
     }
     return count;
  }

  /** Returns the first number which is not a member of the set,
      which falls in the range [min, max).  Returns -1 if all numbers
      in the range are members.
    */
   public long firstNonMember(long min, long max) {

     if (min >= max)
       throw new Error("min must be < max");

     // #### OPTIMIZE THIS!
     while (min < max) {
       if (!member(min))
         return min;
       else
         min++;
     }
     return -1;
  }

  /** Returns the last number which is not a member of the set,
      which falls in the range [min, max).  If all numbers in
      that range are members of the set, returns -1.
    */
   public long lastNonMember(long min, long max) {

     if (min >= max)
       throw new Error("min must be < max");

     // #### OPTIMIZE THIS!
     max--;
     while (max >= min) {
       if (!member(max))
         return max;
       else
         max--;
     }
     return -1;
  }

  /** Returns the first number which is in the set, and which
      is greater than the given value.  Returns -1 if no number
      greater than the given value is in the set.
   */
  public long nextMember(long after) {
    // #### OPTIMIZE THIS!
    long end = max();
    for (long start = after+1; start < end; start++) {
      if (member(start))
         return start;
    }
    return -1;
  }

  /** Returns the last number which is in the set, and which
      is less than the given value.  Returns -1 if the smallest
      member of the set is greater than or equal to the given
      value.
   */
  public long previousMember(long after) {
    // #### OPTIMIZE THIS!
    for (long i = after-1; i >= 0; i--) {
      if (member(i))
         return i;
    }
    return -1;
  }

  /** Re-compresses the data in the set.
      <P>
      The assumption is made that the data in the set is syntactically
      correct (all ranges have a length of at least 1, and all values
      are non-decreasing) but we can optimize the compression, for
      example, merging consecutive literals or ranges into one range.
    */
  private void optimize() {

    int input_size    = data_size;
    int input_length  = data_length;
    long input[]      = data;
    int in            = 0;

    int output_size   = data_size + 1;
    int output_length = 0;
    long output[]     = new long[output_size];
    int out           = 0;

    // We're going to modify the set, so invalidate the cache.
    cached_value = -1;

    while (in < input_length) {
      long from, to;
      boolean range_p = (input[in] < 0);

      if (range_p) {
        // it's a range
        from = input[in+1];
        to = from + (-(input[in+0]));

        // Copy it over
        output[out++] = input[in++];
        output[out++] = input[in++];
      } else {
        // it's a literal
        from = input[in];
        to = from;

        // Copy it over
        output[out++] = input[in++];
      }

      // As long as this chunk is followed by consecutive chunks,
      // keep extending it.
      while (in < input_length &&
             ((input[in] > 0 &&           // literal...
               input[in] == to + 1) ||    // ...and consecutive, or
              (input[in] <= 0 &&          // range...
               input[in+1] == to + 1))    // ...and consecutive.
              ) {
        if (! range_p) {
          // convert the literal to a range.
          out++;
          output[out-2] = 0;
          output[out-1] = from;
          range_p = true;
        }

        if (input[in] > 0) {              // literal
          output[out-2]--;                // increase length by 1
          to++;
          in++;
        } else {
          long L2 = (- input[in]) + 1;
          output[out-2] -= L2;            // increase length by N
          to += L2;
          in += 2;
        }
      }
    }

    data = output;
    data_length = out;
    data_size = output_size;

    // One last pass to turn [N - N+1] into [N, N+1].
    out = 0;
    while (out < output_length) {
      if (output[out] < 0) {
        // it's a range
        if (output[out] == -1) {
          output[out] = output[out+1];
          output[out+1]++;
        }
        out += 2;
      } else {
        // it's a literal
        out++;
      }
    }

    markDirty();
  }

  /** True if there are no elements in the set. */
  protected boolean isEmpty() {
    return (data_length == 0);
  }

  /** Called when a change is made to the set.
      This method does nothing, but is provided for the benefit of
      subclasses.
    */
  public void markDirty() {
  }

  /** Converts a printed representation of the numbers in the set.
      This will be something like <TT>"1-29627,29635,29658,32861-32863"</TT>,
      which is the same representation that <TT>new NewsSet()</TT> expects.
    */
  public void write(ByteBuf out) {
    int i = 0;
    while (i < data_length) {
      long from;
      long to;

      if (i != 0)
        out.append(",");

      if (data[i] < 0) {
        // it's a range
        from = data[i+1];
        to = from + (-(data[i]));
        i += 2;
      } else {
        // it's a literal
        from = data[i];
        to = from;
        i++;
      }

      if (from == 0)
        // See 'kludge' comment above
        from = 1;

      out.append(String.valueOf(from));
      if (from < to) {
        out.append("-");
        out.append(String.valueOf(to));
      }
    }
  }

  public String toString() {
    ByteBuf b = new ByteBuf(data_length * 4);
    write(b);
    return b.toString();
  }



  /*************************************************************************
                                 Self tests
   *************************************************************************/


  protected static void self_test_decoder(String s, String target) {
    String r = new NewsSet(s).toString();
    if (!target.equals(r))
      System.err.println("failed decoder test:\n\t" +
                         s + " =\n\t" + r + " instead of\n\t" + target);
  }

  protected static void self_test_decoder() {
    self_test_decoder ("", "");
    self_test_decoder (" ", "1");
    self_test_decoder ("0", "1");
    self_test_decoder ("1", "1");
    self_test_decoder ("123", "123");
    self_test_decoder (" 123 ", "123");
    self_test_decoder (" 123 4", "123,4");
    self_test_decoder (" 1,2, 3, 4", "1,2,3,4");
    self_test_decoder ("0-70,72-99,100,101", "1-70,72-99,100,101");
    self_test_decoder (" 0-70 , 72 - 99 ,100,101 ", "1-70,72,99,100,101");
    self_test_decoder ("0 - 268435455", "1,268435455");
    self_test_decoder ("0 - 4294967295", "1,4294967295");
    self_test_decoder ("0 - 9223372036854775807", "1,9223372036854775807");
    // This one overflows - we can't help it.
    // self_test_decoder ("0 - 9223372036854775808", "0-9223372036854775808");
  }


  protected static void self_test_adder(NewsSet set,
                                        boolean add_p,
                                        long value,
                                        String target) {
    String old = set.toString();
    if (add_p)
      set.insert(value);
    else
      set.delete(value);
    String s = set.toString();
    if (!s.equals(target))
      System.err.println("failed adder test:\n\t" +
                         old + (add_p ? " + " : " - ") + value +
                         " =\n\t" + s + " instead of\n\t" + target);
  }

  protected static void self_test_adder() {
    NewsSet set = new NewsSet("0-70,72-99,105,107,110-111,117-200");
    self_test_adder(set, true, 205,
                    "1-70,72-99,105,107,110-111,117-200,205");
    self_test_adder(set, true, 206,
                    "1-70,72-99,105,107,110-111,117-200,205-206");
    self_test_adder(set, true, 207,
                    "1-70,72-99,105,107,110-111,117-200,205-207");
    self_test_adder(set, true, 208,
                    "1-70,72-99,105,107,110-111,117-200,205-208");
    self_test_adder(set, true, 208,
                    "1-70,72-99,105,107,110-111,117-200,205-208");
    self_test_adder(set, true, 109,
                    "1-70,72-99,105,107,109-111,117-200,205-208");
    self_test_adder(set, true, 72,
                    "1-70,72-99,105,107,109-111,117-200,205-208");

    self_test_adder(set, false, 205,
                    "1-70,72-99,105,107,109-111,117-200,206-208");
    self_test_adder(set, false, 206,
                    "1-70,72-99,105,107,109-111,117-200,207-208");
    self_test_adder(set, false, 207,
                    "1-70,72-99,105,107,109-111,117-200,208");
    self_test_adder(set, false, 208,
                    "1-70,72-99,105,107,109-111,117-200");
    self_test_adder(set, false, 208,
                    "1-70,72-99,105,107,109-111,117-200");
    self_test_adder(set, false, 109,
                    "1-70,72-99,105,107,110-111,117-200");
    self_test_adder(set, false, 72,
                    "1-70,73-99,105,107,110-111,117-200");

    self_test_adder(set, true, 72,
                    "1-70,72-99,105,107,110-111,117-200");
    self_test_adder(set, true, 109,
                    "1-70,72-99,105,107,109-111,117-200");
    self_test_adder(set, true, 208,
                    "1-70,72-99,105,107,109-111,117-200,208");
    self_test_adder(set, true, 208,
                    "1-70,72-99,105,107,109-111,117-200,208");
    self_test_adder(set, true, 207,
                    "1-70,72-99,105,107,109-111,117-200,207-208");
    self_test_adder(set, true, 206,
                    "1-70,72-99,105,107,109-111,117-200,206-208");
    self_test_adder(set, true, 205,
                    "1-70,72-99,105,107,109-111,117-200,205-208");

    self_test_adder(set, false, 205,
                    "1-70,72-99,105,107,109-111,117-200,206-208");
    self_test_adder(set, false, 206,
                    "1-70,72-99,105,107,109-111,117-200,207-208");
    self_test_adder(set, false, 207,
                    "1-70,72-99,105,107,109-111,117-200,208");
    self_test_adder(set, false, 208,
                    "1-70,72-99,105,107,109-111,117-200");
    self_test_adder(set, false, 208,
                    "1-70,72-99,105,107,109-111,117-200");
    self_test_adder(set, false, 109,
                    "1-70,72-99,105,107,110-111,117-200");
    self_test_adder(set, false, 72,
                    "1-70,73-99,105,107,110-111,117-200");

    self_test_adder(set, true, 100,
                    "1-70,73-100,105,107,110-111,117-200");
    self_test_adder(set, true, 101,
                    "1-70,73-101,105,107,110-111,117-200");
    self_test_adder(set, true, 102,
                    "1-70,73-102,105,107,110-111,117-200");
    self_test_adder(set, true, 103,
                    "1-70,73-103,105,107,110-111,117-200");
    self_test_adder(set, true, 106,
                    "1-70,73-103,105-107,110-111,117-200");
    self_test_adder(set, true, 104,
                    "1-70,73-107,110-111,117-200");
    self_test_adder(set, true, 109,
                    "1-70,73-107,109-111,117-200");
    self_test_adder(set, true, 108,
                    "1-70,73-111,117-200");
  }

  protected static void self_test_ranges(NewsSet set, long start, long end,
                                         String target) {
    String old = set.toString();
    set.insert(start, end);
    String s = set.toString();
    if (!s.equals(target))
      System.err.println("failed range test:\n\t" +
                         old + " + " + start + "-" + end +
                         " =\n\t" + s + " instead of\n\t" + target);
  }


  protected static void self_test_ranges() {
    NewsSet set = new NewsSet("20-40,72-99,105,107,110-111,117-200");
    self_test_ranges(set, 205, 209,
                     "20-40,72-99,105,107,110-111,117-200,205-208");
    self_test_ranges(set, 50, 71,
                     "20-40,50-70,72-99,105,107,110-111,117-200,205-208");
    self_test_ranges(set, 0, 11,
                     "1-10,20-40,50-70,72-99,105,107,110-111,117-200,205-208");
    self_test_ranges(set, 112, 114,
                     "1-10,20-40,50-70,72-99,105,107,110-113,117-200,205-208");
    self_test_ranges(set, 101, 102,
                "1-10,20-40,50-70,72-99,101,105,107,110-113,117-200,205-208");
    self_test_ranges(set, 5, 76,
                     "1-99,101,105,107,110-113,117-200,205-208");
    self_test_ranges(set, 103, 110,
                     "1-99,101,103-113,117-200,205-208");
    self_test_ranges(set, 2, 21,
                     "1-99,101,103-113,117-200,205-208");
    self_test_ranges(set, 1, 10000,
                     "1-9999");
  }


  protected static void self_test_member(NewsSet set, boolean cache, long elt,
                                         boolean target) {
    String old = set.toString();
    if (!cache)
      set.cached_value = -1;
    boolean result = set.member(elt);
    if (target != result)
      System.err.println("failed " + (cache ? "" : "non-") +
                         "cache member test:\n\t" + elt + " in " + old +
                         " = " + result + ", not " + target);
  }

  protected static void self_test_first_nonmember(NewsSet set, boolean cache,
                                                  long start, long end,
                                                  long target) {
    String old = set.toString();
    if (!cache)
      set.cached_value = -1;
    long result = set.firstNonMember(start, end);
    if (target != result)
      System.err.println("failed " + (cache ? "" : "non-") +
                         "cache firstNonMember test:\n\t" +
                         start + "-" + end + " in " +
                         old + " = " + result + ", not " + target);
  }

  protected static void self_test_last_nonmember(NewsSet set, boolean cache,
                                                 long start, long end,
                                                 long target) {
    String old = set.toString();
    if (!cache)
      set.cached_value = -1;
    long result = set.lastNonMember(start, end);
    if (target != result)
      System.err.println("failed " + (cache ? "" : "non-") +
                         "cache lastNonMember test:\n\t" +
                         start + "-" + end + " in " +
                         old + " = " + result + ", not " + target);
  }

  protected static void self_test_next_member(NewsSet set, boolean cache,
                                              long elt, long target) {
    String old = set.toString();
    if (!cache)
      set.cached_value = -1;
    long result = set.nextMember(elt);
    if (target != result)
      System.err.println("failed " + (cache ? "" : "non-") +
                         "cache nextMember test:\n\t" + elt + " in " +
                         old + " = " + result + ", not " + target);
  }

  protected static void self_test_prev_member(NewsSet set, boolean cache,
                                              long elt, long target) {
    String old = set.toString();
    if (!cache)
      set.cached_value = -1;
    long result = set.previousMember(elt);
    if (target != result)
      System.err.println("failed " + (cache ? "" : "non-") +
                         "cache previousMember test:\n\t" + elt + " in " +
                         old + " = " + result + ", not " + target);
  }

  protected static void self_test_member(boolean cache) {
    NewsSet set;

    set = new NewsSet("1-70,72-99,105,107,110-111,117-200");
    self_test_member(set, cache, -1, false);
    self_test_member(set, cache, 0, true);
    self_test_member(set, cache, 1, true);
    self_test_member(set, cache, 20, true);

    set = new NewsSet("0-70,72-99,105,107,110-111,117-200");
    self_test_member(set, cache, -1, false);
    self_test_member(set, cache, 0, true);
    self_test_member(set, cache, 1, true);
    self_test_member(set, cache, 20, true);
    self_test_member(set, cache, 69, true);
    self_test_member(set, cache, 70, true);
    self_test_member(set, cache, 71, false);
    self_test_member(set, cache, 72, true);
    self_test_member(set, cache, 73, true);
    self_test_member(set, cache, 74, true);
    self_test_member(set, cache, 104, false);
    self_test_member(set, cache, 105, true);
    self_test_member(set, cache, 106, false);
    self_test_member(set, cache, 107, true);
    self_test_member(set, cache, 108, false);
    self_test_member(set, cache, 109, false);
    self_test_member(set, cache, 110, true);
    self_test_member(set, cache, 111, true);
    self_test_member(set, cache, 112, false);
    self_test_member(set, cache, 116, false);
    self_test_member(set, cache, 117, true);
    self_test_member(set, cache, 118, true);
    self_test_member(set, cache, 119, true);
    self_test_member(set, cache, 200, true);
    self_test_member(set, cache, 201, false);
    self_test_member(set, cache, 65535, false);
  }

  protected static void self_test_first_nonmember(boolean cache) {
    NewsSet set;

    set = new NewsSet("1-70,72-99,105,107,110-111,117-200");
    self_test_first_nonmember(set, cache, -1, 75,  -1);
    self_test_first_nonmember(set, cache,  0, 32,  -1);
    self_test_first_nonmember(set, cache,  1, 88,  71);
    self_test_first_nonmember(set, cache, 20, 70,  -1);
    self_test_first_nonmember(set, cache, 20, 71,  -1);
    self_test_first_nonmember(set, cache, 20, 72,  71);
    self_test_first_nonmember(set, cache, 20, 500, 71);
    self_test_first_nonmember(set, cache, 71, 90,  71);
    self_test_first_nonmember(set, cache, 72, 90,  -1);

    set = new NewsSet("0-70,72-99,105,107,110-111,117-200");
    self_test_first_nonmember(set, cache, -1,  75,  -1);
    self_test_first_nonmember(set, cache,  0,  32,  -1);
    self_test_first_nonmember(set, cache,  1,  88,  71);
    self_test_first_nonmember(set, cache, 20,  200, 71);
    self_test_first_nonmember(set, cache, 69,  100, 71);
    self_test_first_nonmember(set, cache, 70,  100, 71);
    self_test_first_nonmember(set, cache, 71,  100, 71);
    self_test_first_nonmember(set, cache, 72,  100, -1);
    self_test_first_nonmember(set, cache, 73,  100, -1);
    self_test_first_nonmember(set, cache, 74,  100, -1);
    self_test_first_nonmember(set, cache, 104, 300, 104);
    self_test_first_nonmember(set, cache, 105, 300, 106);
    self_test_first_nonmember(set, cache, 106, 200, 106);
    self_test_first_nonmember(set, cache, 107, 300, 108);
    self_test_first_nonmember(set, cache, 108, 200, 108);
    self_test_first_nonmember(set, cache, 109, 200, 109);
    self_test_first_nonmember(set, cache, 110, 200, 112);
    self_test_first_nonmember(set, cache, 111, 300, 112);
    self_test_first_nonmember(set, cache, 112, 300, 112);
    self_test_first_nonmember(set, cache, 116, 300, 116);
    self_test_first_nonmember(set, cache, 117, 300, 201);
    self_test_first_nonmember(set, cache, 118, 300, 201);
    self_test_first_nonmember(set, cache, 119, 300, 201);
    self_test_first_nonmember(set, cache, 200, 300, 201);
    self_test_first_nonmember(set, cache, 201, 300, 201);
    self_test_first_nonmember(set, cache, 65535, 99999, 65535);
  }

  protected static void self_test_last_nonmember(boolean cache) {
    NewsSet set;

    set = new NewsSet("1-70,72-99,105,107,110-111,117-200");
    self_test_last_nonmember(set, cache, -1, 75,  71);
    self_test_last_nonmember(set, cache,  0, 32,  -1);
    self_test_last_nonmember(set, cache,  1, 88,  71);
    self_test_last_nonmember(set, cache, 20, 70,  -1);
    self_test_last_nonmember(set, cache, 20, 71,  -1);
    self_test_last_nonmember(set, cache, 20, 72,  71);
    self_test_last_nonmember(set, cache, 20, 500, 499);
    self_test_last_nonmember(set, cache, 71, 90,  71);
    self_test_last_nonmember(set, cache, 72, 90,  -1);

    set = new NewsSet("0-70,72-99,105,107,110-111,117-200");
    self_test_last_nonmember(set, cache, -1,  75,  71);
    self_test_last_nonmember(set, cache, 0,   32,  -1);
    self_test_last_nonmember(set, cache, 1,   88,  71);
    self_test_last_nonmember(set, cache, 20,  200, 116);
    self_test_last_nonmember(set, cache, 69,  100, 71);
    self_test_last_nonmember(set, cache, 70,  100, 71);
    self_test_last_nonmember(set, cache, 71,  100, 71);
    self_test_last_nonmember(set, cache, 72,  100, -1);
    self_test_last_nonmember(set, cache, 73,  100, -1);
    self_test_last_nonmember(set, cache, 74,  100, -1);
    self_test_last_nonmember(set, cache, 104, 300, 299);
    self_test_last_nonmember(set, cache, 105, 300, 299);
    self_test_last_nonmember(set, cache, 106, 200, 116);
    self_test_last_nonmember(set, cache, 107, 300, 299);
    self_test_last_nonmember(set, cache, 108, 200, 116);
    self_test_last_nonmember(set, cache, 109, 200, 116);
    self_test_last_nonmember(set, cache, 110, 200, 116);
    self_test_last_nonmember(set, cache, 111, 300, 299);
    self_test_last_nonmember(set, cache, 112, 300, 299);
    self_test_last_nonmember(set, cache, 116, 300, 299);
    self_test_last_nonmember(set, cache, 117, 300, 299);
    self_test_last_nonmember(set, cache, 118, 300, 299);
    self_test_last_nonmember(set, cache, 119, 300, 299);
    self_test_last_nonmember(set, cache, 200, 300, 299);
    self_test_last_nonmember(set, cache, 201, 300, 299);
    self_test_last_nonmember(set, cache, 65535, 99999, 99998);
  }

  protected static void self_test_next_member(boolean cache) {
    NewsSet set;

    set = new NewsSet("1-70,72-99,105,107,110-111,117-200");
    self_test_next_member(set, cache, -1, 0);
    self_test_next_member(set, cache,  0, 1);
    self_test_next_member(set, cache,  1, 2);
    self_test_next_member(set, cache, 20, 21);

    set = new NewsSet("0-70,72-99,105,107,110-111,117-200");
    self_test_next_member(set, cache, -1,  0);
    self_test_next_member(set, cache, 0,   1);
    self_test_next_member(set, cache, 1,   2);
    self_test_next_member(set, cache, 20,  21);
    self_test_next_member(set, cache, 69,  70);
    self_test_next_member(set, cache, 70,  72);
    self_test_next_member(set, cache, 71,  72);
    self_test_next_member(set, cache, 72,  73);
    self_test_next_member(set, cache, 73,  74);
    self_test_next_member(set, cache, 74,  75);
    self_test_next_member(set, cache, 104, 105);
    self_test_next_member(set, cache, 105, 107);
    self_test_next_member(set, cache, 106, 107);
    self_test_next_member(set, cache, 107, 110);
    self_test_next_member(set, cache, 108, 110);
    self_test_next_member(set, cache, 109, 110);
    self_test_next_member(set, cache, 110, 111);
    self_test_next_member(set, cache, 111, 117);
    self_test_next_member(set, cache, 112, 117);
    self_test_next_member(set, cache, 116, 117);
    self_test_next_member(set, cache, 117, 118);
    self_test_next_member(set, cache, 118, 119);
    self_test_next_member(set, cache, 119, 120);
    self_test_next_member(set, cache, 200, -1);
    self_test_next_member(set, cache, 201, -1);
    self_test_next_member(set, cache, 65535, -1);
  }

  protected static void self_test_prev_member(boolean cache) {
    NewsSet set;

    set = new NewsSet("1-70,72-99,105,107,110-111,117-200");
    self_test_prev_member(set, cache, -1, -1);
    self_test_prev_member(set, cache,  0, -1);
    self_test_prev_member(set, cache,  1, 0);
    self_test_prev_member(set, cache, 20, 19);

    set = new NewsSet("0-70,72-99,105,107,110-111,117-200");
    self_test_prev_member(set, cache, -1,  -1);
    self_test_prev_member(set, cache, 0,   -1);
    self_test_prev_member(set, cache, 1,   0);
    self_test_prev_member(set, cache, 20,  19);
    self_test_prev_member(set, cache, 69,  68);
    self_test_prev_member(set, cache, 70,  69);
    self_test_prev_member(set, cache, 71,  70);
    self_test_prev_member(set, cache, 72,  70);
    self_test_prev_member(set, cache, 73,  72);
    self_test_prev_member(set, cache, 74,  73);
    self_test_prev_member(set, cache, 104, 99);
    self_test_prev_member(set, cache, 105, 99);
    self_test_prev_member(set, cache, 106, 105);
    self_test_prev_member(set, cache, 107, 105);
    self_test_prev_member(set, cache, 108, 107);
    self_test_prev_member(set, cache, 109, 107);
    self_test_prev_member(set, cache, 110, 107);
    self_test_prev_member(set, cache, 111, 110);
    self_test_prev_member(set, cache, 112, 111);
    self_test_prev_member(set, cache, 116, 111);
    self_test_prev_member(set, cache, 117, 111);
    self_test_prev_member(set, cache, 118, 117);
    self_test_prev_member(set, cache, 119, 118);
    self_test_prev_member(set, cache, 200, 199);
    self_test_prev_member(set, cache, 201, 200);
    self_test_prev_member(set, cache, 202, 200);
    self_test_prev_member(set, cache, 203, 200);
    self_test_prev_member(set, cache, 300, 200);
    self_test_prev_member(set, cache, 65535, 200);
  }

  protected static void self_test() {
    self_test_decoder();
    self_test_adder();
    self_test_ranges();
    self_test_member(false);
    self_test_member(true);
    self_test_first_nonmember(true);
    self_test_first_nonmember(false);
    self_test_last_nonmember(true);
    self_test_last_nonmember(false);
    self_test_next_member(true);
    self_test_next_member(false);
    self_test_prev_member(true);
    self_test_prev_member(false);
  }

  public static void main(String args[]) {
    self_test();
  }
}
