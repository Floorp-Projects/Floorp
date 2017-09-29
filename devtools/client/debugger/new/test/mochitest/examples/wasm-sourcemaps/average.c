/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

extern float average(signed* numbers, unsigned length);
extern signed sum(signed* numbers, unsigned length);

signed __attribute__((noinline)) sum(signed* numbers, unsigned length)
{
  signed result;
  unsigned i;
  result = 0;
  for (i = 0; i < length; i++)
    result += numbers[i];
  return result;
}

float average(signed* numbers, unsigned length)
{
  float s = (float)sum(numbers, length);
  return s / length;
}
