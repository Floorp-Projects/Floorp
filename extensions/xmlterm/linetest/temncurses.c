
  /* Move cursor and display string */
  x = 2;
  y = 4;

  /* if (scanf("%d %d", &y, &x) == EOF) break; */

  /* Display string at specified location and advance cursor */
  mvaddnstr(y, x, "test 1", 6);

  attron(A_BOLD);
  mvaddnstr(y, x+6, "test2 ", 6);
  attroff(A_BOLD);

  refresh();

  for (;;) {
    c = getch();
    if (c == KEY_MOUSE) {
      if (getmouse(&mev) == OK) {
        move(mev.y, mev.x);
        addnstr("MEV ", 4);
        refresh();

        /* Delete top line, and move up (if +1, insert top line, move down) */
        move(0,0);
        insdelln(-1);

      }
    }
  }

