// don't assert
for (a = 0; a < 9; ++a) {
    M: for (let c in <x>></x>) {
      break M
    }
}
