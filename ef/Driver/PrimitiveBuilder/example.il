/* 
 * void function(int res)
 * {
 *   int i, j;
 *   j = 0;
 *   for (i=0; i < 100; i++)
 *    j += i;
 *   return res;
 * }
 * 
 */

/*
start:
	vi2  := Arg_I arg0
    wi3  := Const_I 1
N2:
	vi4  := Phi_I  wi3, vi8
	vi5  := Phi_I  vi2, vi5

	vc7  := Cmp_I  vi4, 100
	IfLt  vc7, N4
	vi8  := Add_I  vi4, 1
	Branch N2

N4:
	vi9   := Mul_I  vi4, 3
	vi10  := Add_I  vi9, 2
	vi11  := Mul_I  vi5, 6
	vi12  := Add_I  vi10, vi11
	Result_I vi12

*/

start:
		v0 := Arg_I arg0
		i0 := Const_I 0
		j0 := Const_I 0
		Branch test

loop:
		j1 := Add_I i1, j2
		i2 := Add_I i1, 1

test:
		i1 := Phi_I i0, i2
		j2 := Phi_I j0, j1
		c := Cmp_I  i1, 100
		IfLt c, loop

		Result_I v0
