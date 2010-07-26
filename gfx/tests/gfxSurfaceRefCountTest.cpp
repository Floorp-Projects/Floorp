#include <stdio.h>

#include "gfxASurface.h"
#include "gfxImageSurface.h"

#include "cairo.h"

int
GetASurfaceRefCount(gfxASurface *s) {
    NS_ADDREF(s);
    return s->Release();
}

int
CheckInt (int value, int expected) {
    if (value != expected) {
        fprintf (stderr, "Expected %d got %d\n", expected, value);
        return 1;
    }

    return 0;
}

int
CheckPointer (void *value, void *expected) {
    if (value != expected) {
        fprintf (stderr, "Expected %p got %p\n", expected, value);
        return 1;
    }

    return 0;
}

static cairo_user_data_key_t destruction_key;
void
SurfaceDestroyNotifier (void *data) {
    *(int *)data = 1;
}

int
TestNewSurface () {
    int failures = 0;
    int destroyed = 0;

    nsRefPtr<gfxASurface> s = new gfxImageSurface (gfxIntSize(10, 10), gfxASurface::ImageFormatARGB32);
    cairo_surface_t *cs = s->CairoSurface();

    cairo_surface_set_user_data (cs, &destruction_key, &destroyed, SurfaceDestroyNotifier);

    failures += CheckInt (GetASurfaceRefCount(s.get()), 1);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 1);
    failures += CheckInt (destroyed, 0);

    cairo_surface_reference(cs);

    failures += CheckInt (GetASurfaceRefCount(s.get()), 2);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 2);
    failures += CheckInt (destroyed, 0);

    gfxASurface *savedWrapper = s.get();

    s = nsnull;

    failures += CheckInt (cairo_surface_get_reference_count(cs), 1);
    failures += CheckInt (destroyed, 0);

    s = gfxASurface::Wrap(cs);

    failures += CheckPointer (s.get(), savedWrapper);
    failures += CheckInt (GetASurfaceRefCount(s.get()), 2);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 2);
    failures += CheckInt (destroyed, 0);

    cairo_surface_destroy(cs);

    failures += CheckInt (GetASurfaceRefCount(s.get()), 1);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 1);
    failures += CheckInt (destroyed, 0);

    s = nsnull;

    failures += CheckInt (destroyed, 1);

    return failures;
}

int
TestExistingSurface () {
    int failures = 0;
    int destroyed = 0;

    cairo_surface_t *cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);

    cairo_surface_set_user_data (cs, &destruction_key, &destroyed, SurfaceDestroyNotifier);

    failures += CheckInt (cairo_surface_get_reference_count(cs), 1);
    failures += CheckInt (destroyed, 0);

    nsRefPtr<gfxASurface> s = gfxASurface::Wrap(cs);

    failures += CheckInt (GetASurfaceRefCount(s.get()), 2);

    cairo_surface_reference(cs);

    failures += CheckInt (GetASurfaceRefCount(s.get()), 3);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 3);
    failures += CheckInt (destroyed, 0);

    gfxASurface *savedWrapper = s.get();

    s = nsnull;

    failures += CheckInt (cairo_surface_get_reference_count(cs), 2);
    failures += CheckInt (destroyed, 0);

    s = gfxASurface::Wrap(cs);

    failures += CheckPointer (s.get(), savedWrapper);
    failures += CheckInt (GetASurfaceRefCount(s.get()), 3);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 3);
    failures += CheckInt (destroyed, 0);

    cairo_surface_destroy(cs);

    failures += CheckInt (GetASurfaceRefCount(s.get()), 2);
    failures += CheckInt (cairo_surface_get_reference_count(cs), 2);
    failures += CheckInt (destroyed, 0);

    s = nsnull;

    failures += CheckInt (cairo_surface_get_reference_count(cs), 1);
    failures += CheckInt (destroyed, 0);

    cairo_surface_destroy(cs);

    failures += CheckInt (destroyed, 1);

    return failures;
}

int
main (int argc, char **argv) {
    int fail;

    fail = TestNewSurface();
    fprintf (stderr, "TestNewSurface: %d failures\n", fail);
    fail = TestExistingSurface();
    fprintf (stderr, "TestExistingSurface: %d failures\n", fail);
}

