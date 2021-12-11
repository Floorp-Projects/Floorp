/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Copyright 1992,1993 Simmule Turner and Rich Salz.  All rights reserved.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 * 1. The authors are not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 * 4. This notice may not be removed or altered.
 */


/*
**  Unix system-dependant routines for editline library.
*/
#include "editline.h"

#if	defined(HAVE_TCGETATTR)
#include <termios.h>

void
rl_ttyset(Reset)
    int				Reset;
{
    static struct termios	old;
    struct termios		new;

    if (Reset == 0) {
	(void)tcgetattr(0, &old);
	rl_erase = old.c_cc[VERASE];
	rl_kill = old.c_cc[VKILL];
	rl_eof = old.c_cc[VEOF];
	rl_intr = old.c_cc[VINTR];
	rl_quit = old.c_cc[VQUIT];

	new = old;
	new.c_cc[VINTR] = -1;
	new.c_cc[VQUIT] = -1;
	new.c_lflag &= ~(ECHO | ICANON);
	new.c_iflag &= ~(ISTRIP | INPCK);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	(void)tcsetattr(0, TCSADRAIN, &new);
    }
    else
	(void)tcsetattr(0, TCSADRAIN, &old);
}

#else
#if	defined(HAVE_TERMIO)
#include <termio.h>

void
rl_ttyset(Reset)
    int				Reset;
{
    static struct termio	old;
    struct termio		new;

    if (Reset == 0) {
	(void)ioctl(0, TCGETA, &old);
	rl_erase = old.c_cc[VERASE];
	rl_kill = old.c_cc[VKILL];
	rl_eof = old.c_cc[VEOF];
	rl_intr = old.c_cc[VINTR];
	rl_quit = old.c_cc[VQUIT];

	new = old;
	new.c_cc[VINTR] = -1;
	new.c_cc[VQUIT] = -1;
	new.c_lflag &= ~(ECHO | ICANON);
	new.c_iflag &= ~(ISTRIP | INPCK);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	(void)ioctl(0, TCSETAW, &new);
    }
    else
	(void)ioctl(0, TCSETAW, &old);
}

#else
#include <sgtty.h>

void
rl_ttyset(Reset)
    int				Reset;
{
    static struct sgttyb	old_sgttyb;
    static struct tchars	old_tchars;
    struct sgttyb		new_sgttyb;
    struct tchars		new_tchars;

    if (Reset == 0) {
	(void)ioctl(0, TIOCGETP, &old_sgttyb);
	rl_erase = old_sgttyb.sg_erase;
	rl_kill = old_sgttyb.sg_kill;

	(void)ioctl(0, TIOCGETC, &old_tchars);
	rl_eof = old_tchars.t_eofc;
	rl_intr = old_tchars.t_intrc;
	rl_quit = old_tchars.t_quitc;

	new_sgttyb = old_sgttyb;
	new_sgttyb.sg_flags &= ~ECHO;
	new_sgttyb.sg_flags |= RAW;
#if	defined(PASS8)
	new_sgttyb.sg_flags |= PASS8;
#endif	/* defined(PASS8) */
	(void)ioctl(0, TIOCSETP, &new_sgttyb);

	new_tchars = old_tchars;
	new_tchars.t_intrc = -1;
	new_tchars.t_quitc = -1;
	(void)ioctl(0, TIOCSETC, &new_tchars);
    }
    else {
	(void)ioctl(0, TIOCSETP, &old_sgttyb);
	(void)ioctl(0, TIOCSETC, &old_tchars);
    }
}
#endif	/* defined(HAVE_TERMIO) */
#endif	/* defined(HAVE_TCGETATTR) */

void
rl_add_slash(path, p)
    char	*path;
    char	*p;
{
    struct stat	Sb;

    if (stat(path, &Sb) >= 0)
	(void)strcat(p, S_ISDIR(Sb.st_mode) ? "/" : " ");
}

