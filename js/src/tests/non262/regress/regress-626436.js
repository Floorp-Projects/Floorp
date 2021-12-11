// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributors: Christian Holler <decoder@own-hero.net>, Jesse Ruderman <jruderman@gmail.com>

(1 ? 2 : delete(0 ? 0 : {})).x;

reportCompare(0, 0, 'ok');
