#include "xp.h"
#include "xpgetstr.h"

#include <signal.h>
#include <pwd.h>

#include "nspr.h"

extern int GNOMEFE_APP_HAS_DETECTED_LOCK;
extern int GNOMEFE_ANOTHER_USER_IS_RUNNING_APP;
extern int GNOMEFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY;
extern int GNOMEFE_EXISTS_BUT_UNABLE_TO_RENAME;
extern int GNOMEFE_UNABLE_TO_CREATE_DIRECTORY;
extern int GNOMEFE_UNKNOWN_ERROR;
extern int GNOMEFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID;
extern int GNOMEFE_YOU_MAY_CONTINUE_TO_USE;
extern int GNOMEFE_OTHERWISE_CHOOSE_CANCEL;

char *fe_config_dir;
char *fe_home_dir;
char *fe_pidlock;

#define MAXTRIES	100

static XP_Bool
fe_ensure_config_dir_exists ()
{
  char *dir, *fmt;
  static char buf [2048];
  struct stat st;
  XP_Bool exists;

  dir = PR_smprintf ("%s/%s", fe_home_dir, MOZ_USER_DIR);
  if (!dir)
    return FALSE;

  exists = !stat (dir, &st);

  if (exists && !(st.st_mode & S_IFDIR))
    {
      /* It exists but is not a directory!
	 Rename the old file so that we can create the directory.
	 */
      char *loser = (char *) XP_ALLOC (XP_STRLEN (dir) + 100);
      XP_STRCPY (loser, dir);
      XP_STRCAT (loser, ".BAK");
      if (rename (dir, loser) == 0)
	{
	  fmt = XP_GetString( GNOMEFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY );
	  exists = FALSE;
	}
      else
	{
	  fmt = XP_GetString( GNOMEFE_EXISTS_BUT_UNABLE_TO_RENAME );
	}

      PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, dir, loser);
      XP_FREE (loser);
      FE_Alert(FE_GetInitContext(), buf);
      if (exists)
	{
	  free (dir);
	  return FALSE;
	}
    }

  if (!exists)
    {
      /* ~/.netscape/ does not exist.  Create the directory.
       */
      if (mkdir (dir, 0700) < 0)
	{
	  fmt = XP_GetString( GNOMEFE_UNABLE_TO_CREATE_DIRECTORY );
	  PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, dir,
		   ((errno >= 0 && errno < sys_nerr)
		    ? sys_errlist [errno] : XP_GetString( GNOMEFE_UNKNOWN_ERROR )));
	  FE_Alert(FE_GetInitContext(), buf);
	  free (dir);
	  return FALSE;
	}
    }

  fe_config_dir = dir;
  return TRUE;
}

static int
fe_create_pidlock (const char *name, unsigned long *paddr, pid_t *ppid)
{
  char hostname[64];		/* should use MAXHOSTNAMELEN */
  PRHostEnt *hp, hpbuf;
  char dbbuf[PR_NETDB_BUF_SIZE];
  unsigned long myaddr, addr;
  struct in_addr inaddr;
  char *signature;
  int len, tries, rval;
  char buf[1024];		/* should use PATH_MAX or pathconf */
  char *colon, *after;
  pid_t pid;

  {
  PRStatus sts;

  sts = PR_GetSystemInfo(PR_SI_HOSTNAME, hostname, sizeof(hostname));
  if (sts == PR_FAILURE)
    inaddr.s_addr = INADDR_LOOPBACK;
  else {
    sts = PR_GetHostByName (hostname, dbbuf, sizeof(dbbuf), &hpbuf);
    if (sts == PR_FAILURE)
      inaddr.s_addr = INADDR_LOOPBACK;
    else {
      hp = &hpbuf;
      memcpy (&inaddr, hp->h_addr, sizeof inaddr);
    }
  }
  }

  myaddr = inaddr.s_addr;
  signature = PR_smprintf ("%s:%u", inet_ntoa (inaddr), (unsigned)getpid ());
  tries = 0;
  addr = 0;
  pid = 0;
  while ((rval = symlink (signature, name)) < 0)
    {
      if (errno != EEXIST)
	break;
      len = readlink (name, buf, sizeof buf - 1);
      if (len > 0)
	{
	  buf[len] = '\0';
	  colon = strchr (buf, ':');
	  if (colon != NULL)
	    {
	      *colon++ = '\0';
	      if ((addr = inet_addr (buf)) != (unsigned long)-1)
		{
		  pid = strtol (colon, &after, 0);
		  if (pid != 0 && *after == '\0')
		    {
		      if (addr != myaddr)
			{
			  /* Remote pid: give up even if lock is stuck. */
			  break;
			}

		      /* signator was a local process -- check liveness */
		      if (kill (pid, 0) == 0 || errno != ESRCH)
			{
			  /* Lock-owning process appears to be alive. */
			  break;
			}
		    }
		}
	    }
	}

      /* Try to claim a bogus or stuck lock. */
      (void) unlink (name);
      if (++tries > MAXTRIES)
	break;
    }

  if (rval == 0)
    {
      struct sigaction act, oldact;

      act.sa_handler = exit; /* XXXX */
      act.sa_flags = 0;
      sigfillset (&act.sa_mask);

      /* Set SIGINT, SIGTERM and SIGHUP to our fe_Exit(). If these signals
       * have already been ignored, dont install our handler because we
       * could have been started up in the background with a nohup.
       */
      sigaction (SIGINT, NULL, &oldact);
      if (oldact.sa_handler != SIG_IGN)
	sigaction (SIGINT, &act, NULL);

      sigaction (SIGHUP, NULL, &oldact);
      if (oldact.sa_handler != SIG_IGN)
	sigaction (SIGHUP, &act, NULL);

      sigaction (SIGTERM, NULL, &oldact);
      if (oldact.sa_handler != SIG_IGN)
	sigaction (SIGTERM, &act, NULL);

#ifndef SUNOS4
      /* atexit() is not available in sun4. We need to find someother
       * mechanism to do this in sun4. Maybe we will get a signal or
       * something.
       */

#if 0
      /* Register a atexit() handler to remove lock file */
      atexit(fe_atexit_handler);
#endif
#endif /* SUNOS4 */
    }
  free (signature);
  *paddr = addr;
  *ppid = pid;
  return rval;
}

void check_for_lock_file()
{
  /* See if another instance is running.  If so, don't use the .db files */
  if (fe_ensure_config_dir_exists ())
    {
      char *name;
      unsigned long addr;
      pid_t pid;

      name = PR_smprintf ("%s/lock", fe_config_dir);
      addr = 0;
      pid = 0;
      if (name == NULL)
	/* out of memory -- what to do? */;
      else if (fe_create_pidlock (name, &addr, &pid) == 0)
	{
	  /* Remember name in fe_pidlock so we can unlink it on exit. */
	  fe_pidlock = name;

	  /* Remember that we have access to the databases.
	   * This lets us know when we can enable the history window. */
#if 0
	  fe_globalData.all_databases_locked = FALSE;
#endif
	}
      else
	{
	  char *fmt = NULL;
	  char *lock = name ? name : MOZ_USER_DIR "/lock";

	  fmt = PR_sprintf_append(fmt, XP_GetString(GNOMEFE_APP_HAS_DETECTED_LOCK),
				  XP_AppName, lock);
	  fmt = PR_sprintf_append(fmt,
				  XP_GetString(GNOMEFE_ANOTHER_USER_IS_RUNNING_APP),
				  XP_AppName,
				  fe_config_dir);

	  if (addr != 0 && pid != 0)
	    {
	      struct in_addr inaddr;
	      PRHostEnt *hp, hpbuf;
	      char dbbuf[PR_NETDB_BUF_SIZE];
	      const char *host;

	      inaddr.s_addr = addr;
              {
	      PRStatus sts;
	      PRNetAddr pr_addr;

	      pr_addr.inet.family = AF_INET;
	      pr_addr.inet.ip = addr;
	      sts = PR_GetHostByAddr(&pr_addr,dbbuf, sizeof(dbbuf), &hpbuf);
	      if (sts == PR_FAILURE)
		  hp = NULL;
	      else
		  hp = &hpbuf;
	      }
	      host = (hp == NULL) ? inet_ntoa(inaddr) : hp->h_name;
	      fmt = PR_sprintf_append(fmt,
		      XP_GetString(GNOMEFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID),
		      host, (unsigned)pid);
	    }

	  fmt = PR_sprintf_append(fmt,
				  XP_GetString(GNOMEFE_YOU_MAY_CONTINUE_TO_USE),
				  XP_AppName);
	  fmt = PR_sprintf_append(fmt,
				  XP_GetString(GNOMEFE_OTHERWISE_CHOOSE_CANCEL),
				  XP_AppName, lock, XP_AppName);

	  if (fmt)
	    {
	      if (!FE_Confirm(FE_GetInitContext(), fmt))
		exit (0);
	      free (fmt);
	    }
	  if (name) free (name);

	  /* Keep on-disk databases from being open-able. */
	  dbSetOrClearDBLock (LockOutDatabase);
#if 0
	  fe_globalData.all_databases_locked = TRUE;
#endif
	}
    }
}
