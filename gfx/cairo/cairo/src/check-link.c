#define CAIRO_VERSION_H 1

#include <cairo.h>

/* get the "real" version info instead of dummy cairo-version.h */
#undef CAIRO_VERSION_H
#include "../cairo-version.h"

#include <stdio.h>

int
main (void)
{
  printf ("Check linking to the just built cairo library\n");
  if (cairo_version () == CAIRO_VERSION) {
    return 0;
  } else {
    fprintf (stderr,
	     "Error: linked to cairo version %s instead of %s\n",
	     cairo_version_string (),
	     CAIRO_VERSION_STRING);
    return 1;
  }
}
