<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

#Getting Started
The contribution process consists of a number of steps. First, you need to get
a copy of the code. Next, you need to open a bug for the bug or feature you want
to work on, and assign it to yourself. Alternatively, you can take an existing
bug to work on. Once you've taken a bug, you can start writing a patch. Once
your patch is complete, you've made sure it doesn't break any tests, and you've
gotten a positive review for it, the last step is to request for your patch to
be merged with the main codebase.

Although these individual steps are all obvious, there are quite some details
involved. The rest of this article will cover each individual step of the
contribution process in more detail.

##Getting the Code
The Add-on SDK code is hosted on GitHub. GitHub is a web-based hosting service
for software projects that is based on Git, a distributed version control
system. Both GitHub and Git are an integral part of our workflow. If you haven't
familiarized yourself with Git before, I strongly suggest you do so now. You're
free to ignore that suggestion if you want, but it's going to hurt you later on
(don't come crying to me if you end up accidentally detaching your head, for
instance). A full explanation of how to use Git is out of scope for this
document, but a very good one
[can be found online here](http://git-scm.com/book). Reading at least sections
1-3 from that book should be enough to get you started.

If you're already familiar with Git, or if you decided to ignore my advice and
jump right in, the following steps will get you a local copy of the Add-on SDK
code on your machine:

1. Fork the SDK repository to your GitHub account
2. Clone the forked repository to your machine

A fork is similar to a clone in that it creates a complete copy of a repository,
including the history of every file. The difference is that a fork copies the
repository to your GitHub account, whereas a clone copies it to your machine. To
create a fork of the SDK repository, you need a GitHub account. If you don't
already have one, you can [create one here](https://github.com/) (don't worry:
it's free!). Once you got yourself an account, go to
[the Add-on SDK repository](https://github.com/mozilla/addon-sdk), and click the
fork button in the upper-right corner. This will start the forking process.
This could take anywhere between a couple of seconds and a couple of minutes.

Once the forking process is complete, the forked repository will be available at
https://github.com/\<your-username\>/addon-sdk. To create a clone of the this
repository, you need to have Git installed on your machine. If you don’t have it
already, you can [download it here](http://git-scm.com/). Once you have Git
installed (make sure you also configured your name and e-mail
address), open your terminal, and enter the following command from the directory
where you want to have the clone stored:

> `git clone ssh://github.com/<your-username>/addon-sdk`

This will start the cloning process. Like the forking process, this could take
anywhere between a couple of seconds and a couple of minutes, depending on the
speed of your connection.

If you did everything correctly so far, once the cloning process is complete,
the cloned repository will have been stored inside the directory from which you
ran the clone command, in a new directory called addon-sdk. Now we can start
working with it. Yay!

As a final note: it is possible to skip step 1, and clone the SDK repository
directly to your machine. This is useful if you only want to study the SDK code.
However, if your goal is to actually contribute to the SDK, skipping step 1 is a
bad idea, because you won’t be able to make pull requests in that case.

##Opening a Bug
In any large software project, keeping track of bugs is crucially important.
Without it, developers wouldn't be able to answer questions such as: what do I
need to work on, who is working on what, etc. Mozilla uses its own web-based,
general-purpose bugtracker, called Bugzilla, to keep track of bugs. Like GitHub
and Git, Bugzilla is an integral part of our workflow. When you discover a new
bug, or want to implement a new feature, you start by creating an entry for it
in Bugzilla. By doing so, you give the SDK team a chance to confirm whether your
bug isn't actually a feature, or your feature isn't actually a bug
(that is, a feature we feel doesn't belong into the SDK).

Within Bugzilla, the term _bug_ is often used interchangably to refer to both
bugs and features. Similarly, the Bugzilla entry for a bug is also named bug,
and the process of creating it is known as _opening a bug_. It is important that
you understand this terminology, as other people will regularly refer to it.

I really urge you to open a bug first and wait for it to get confirmed before
you start working on something. Nothing sucks more than your patch getting
rejected because we felt it shouldn't go into the SDK. Having this discussion
first saves you from doing useless work. If you have questions about a bug, but
don't know who to ask (or the person you need to ask isn't online), Bugzilla is
the communication channel of choice. When you open a bug, the relevant people
are automatically put on the cc-list, so they will get an e-mail every time you
write a comment in the bug.

To open a bug, you need a Bugzilla account. If you don't already have one, you
can [create it here](https://bugzilla.mozilla.org/). Once you got yourself an
account, click the "new" link in the upper-left corner. This will take you to a
page where you need to select the product that is affected by your bug. It isn't
immediately obvious what you should pick here (and with not immediately obvious
I mean completely non-obvious), so I'll just point it out to you: as you might
expect, the Add-on SDK is listed under "Other products", at the bottom of the
page.

After selecting the Add-on SDK, you will be taken to another page, where you
need to fill out the details for the bug. The important fields are the component
affected by this bug, the summary, and a short description of the bug (don't
worry about coming up with the perfect description for your bug. If something is
not clear, someone from the SDK team will simply write a comment asking for
clarification). The other fields are optional, and you can leave them as is, if
you so desire.

Note that when you fill out the summary field, Bugzilla automatically looks for
bugs that are possible duplicates of the one you're creating. If you spot such a
duplicate, there's no need to create another bug. In fact, doing so is
pointless, as duplicate bugs are almost always immediately closed. Don't worry
about accidentally opening a duplicate bug though. Doing so is not considered a
major offense (unless you do it on purpose, of course).

After filling out the details for the bug, the final step is to click the
"Submit Bug" button at the bottom of the page. Once you click this button, the
bug will be stored in Bugzilla’s database, and the creation process is
completed. The initial status of your bug will be `UNCONFIRMED`. All you need to
do now is wait for someone from the SDK team to change the status to either
`NEW` or `WONTFIX`.

##Taking a Bug
Since this is a contributor's guide, I've assumed until now that if you opened a
bug, you did so with the intention of fixing it. Simply because you're the one
that opened it doesn't mean you have to fix a bug, however. Conversely, simply
because you're _not_ the one that opened it doesn't mean you can't fix a bug. In
fact, you can work on any bug you like, provided nobody else is already working
on it. To check if somebody is already working on a bug, go to the entry for
that bug and check the "Assigned To" field. If it says "Nobody; OK to take it
and work on it", you're good to go: you can assign the bug to yourself by
clicking on "(take)" right next to it.

Keep in mind that taking a bug to creates the expectation that you will work on
it. It's perfectly ok to take your time, but if this is the first bug you're
working on, you might want to make sure that this isn't something that has very
high priority for the SDK team. You can do so by checking the importance field
on the bug page (P1 is the highest priority). If you've assigned a bug to
yourself that looked easy at the time, but turns out to be too hard for you to
fix, don't feel bad! It happens to all of us. Just remove yourself as the
assignee for the bug, and write a comment explaining why you're no longer able
to work on it, so somebody else can take a shot at it.

A word of warning: taking a bug that is already assigned to someone else is
considered extremely rude. Just imagine yourself working hard on a series of
patches, when suddenly this jerk comes out of nowhere and submits his own
patches for the bug. Not only is doing so an inefficient use of time, it also
shows a lack of respect for other the hard work of other contributors. The other
side of the coin is that contributors do get busy every now and then, so if you
stumble upon a bug that is already assigned to someone else but hasn't shown any
activity lately, chances are the person to which the bug is assigned will gladly
let you take it off his/her hands. The general rule is to always ask the person
assigned to the bug if it is ok for you to take it.

As a final note, if you're not sure what bug to work on, or having a hard time
finding a bug you think you can handle, a useful tip is to search for the term
"good first bug". Bugs that are particularly easy, or are particularly well
suited to familiarize yourself with the SDK, are often given this label by the
SDK team when they're opened.

##Writing a Patch
Once you've taken a bug, you're ready to start doing what you really want to do:
writing some code. The changes introduced by your code are known as a patch.
Your goal, of course, is to get this patch landed in the main SDK repository. In
case you aren't familiar with git, the following command will cause it to
generate a diff:

> `git diff`

A diff describes all the changes introduced by your patch. These changes are not
yet final, since they are not yet stored in the repository. Once your patch is
complete, you can _commit_ it to the repository by writing:

> `git commit`

After pressing enter, you will be prompted for a commit message. What goes in
the commit message is more or less up to you, but you should at least include
the bug number and a short summary (usually a single line) of what the patch
does. This makes it easier to find your commit later on.

It is considered good manners to write your code in the same style as the rest
of a file. It doesn't really matter what coding style you use, as long as it's
consistent. The SDK might not always use the exact same coding style for each
file, but it strives to be as consistent as possible. Having said that: if
you're not completely sure what coding style to use, just pick something and
don't worry about it. If the rest of the file doesn't make it clear what you
should do, it most likely doesn't matter.

##Making a Pull Request
To submit a patch for review, you need to make a pull request. Basically, a pull
request is a way of saying: "Hey, I've created this awesome patch on top of my
fork of the SDK repository, could you please merge it with the global 
repository?". GitHub has built-in support for pull requests. However, you can
only make pull requests from repositories on your GitHub account, not from
repositories on your local machine. This is why I told you to fork the SDK
repository to your GitHub account first (you did listen to me, didn't you?).

In the previous section, you commited your patch to your local repository, so
here, the next step is to synchronize your local repository with the remote one,
by writing:

> `git push`

This pushes the changes from your local repository into the remote repository.
As you might have guessed, a push is the opposite of a pull, where somebody else
pulls changes from a remote repository into their own repository (hence the term
'pull request'). After pressing enter, GitHub will prompt you for your username
and password before actually allowing the push.

If you did everything correctly up until this point, your patch should now show
up in your remote repository (take a look at your repository on GitHub to make
sure). We're now ready to make a pull request. To do so, go to your repository
on GitHub and click the "Pull Request" button at the top of the page. This will
take you to a new page, where you need to fill out the title of your pull
request, as well as a short description of what the patch does. As we said
before, it is common practice to at least include the bug number and a short
summary in the title. After you've filled in both fields, click the "Send Pull
Request" button.

That's it, we're done! Or are we? This is software development after all, so
we'd expect there to be at least one redundant step. Luckily, there is such a
step, because we also have to submit our patch for review on Bugzilla. I imagine
you might be wondering to yourself right now: "WHY???". Let me try to explain.
The reason we have this extra step is that most Mozilla projects use Mercurial
and Bugzilla as their version control and project management tool, respectively.
To stay consistent with the rest of Mozilla, we provide a Mercurial mirror of
our Git repository, and submit our patches for review in both GitHub and
Bugzilla.

If that doesn't make any sense to you, that's ok: it doesn't to me, either. The
good news, however, is that you don't have to redo all the work you just did.
Normally, when you want to submit a patch for review on Bugzilla, you have to
create a diff for the patch and add it as an attachment to the bug (if you still
haven't opened one, this would be the time to do it). However, these changes are
also described by the commit of your patch, so its sufficient to attach a file
that links to the pull request. To find the link to your pull request, go to
your GitHub account and click the "Pull Requests" button at the top. This will
take you to a list of your active pull requests. You can use the template here
below as your attachment. Simply copy the link to your pull request, and use it
to replace all instances of \<YOUR_LINK_HERE\>:

    <!DOCTYPE html>
    <meta charset="utf-8">
    <meta http-equiv="refresh" content="<YOUR_LINK_HERE>">
    <title>Bugzilla Code Review</title>
    <p>You can review this patch at <a href="<YOUR_LINK_HERE >"><YOUR_LINK_HERE></a>,
    or wait 5 seconds to be redirected there automatically.</p>

Finally, to add the attachment to the bug, go to the bug in Bugzilla, and click
on "Add an attachment" right above the comments. Make sure you fill out a
description for the attachment, and to set the review flag to '?' (you can find
a list of reviewers on
[this page](https://github.com/mozilla/addon-sdk/wiki/contribute)). The '?' here
means that you're making a request. If your patch gets a positive review, the
reviewer will set this flag to '+'. Otherwise, he/she will set it to '-', with
some feedback on why your patch got rejected. Of course, since we also use
GitHub for our review process, you're most likely to get your feedback there,
instead of Bugzilla. If your patch didn't get a positive review right away,
don't sweat it. If you waited for your bug to get confirmed before submitting
your patch, you'll usually only have to change a few small things to get a
positive review for your next attempt. Once your patch gets a positive review,
you don't need to do anything else. Since you did a pull request, it will
automatically be merged into the remote repository, usually by the person that
reviewed your patch.

##Getting Additional Help
If something in this article wasn't clear to you, or if you need additional
help, the best place to go is irc. Mozilla relies heavily on irc for direct
communication between contributors. The SDK team hangs out on the #jetpack
channel on the irc.mozilla.org server (Jetpack was the original name of the
SDK, in case you're wondering).

Unless you are know what you are doing, it can be hard to get the information
you need from irc, uso here are a few useful tips:

* Mozilla is a global organization, with contributors all over the world, so the
  people you are trying to reach are likely not in the same timezone as you.
  Most contributors to the SDK are currently based in the US, so if you're in
  Europe, and asking a question on irc in the early afternoon, you're not likely
  to get many replies.

* Most members of the SDK team are also Mozilla employees, which means they're
  often busy doing other stuff. That doesn't mean they don't want to help you.
  On the contrary: Mozilla encourages employees to help out contributors
  whenever they can. But it does mean that we're sometimes busy doing other
  things than checking irc, so your question may go unnoticed. If that happens,
  the best course of action is often to just ask again.

* If you direct your question to a specific person, rather than the entire
  channel, your chances of getting an answer are a lot better. If you prefix
  your message with that person's irc name, he/she will get a notification in
  his irc client. Try to make sure that the person you're asking is actually the
  one you need, though. Don't just ask random questions to random persons in the
  hopes you'll get more response that way.

* If you're not familiar with irc, a common idiom is to send someone a message
  saying "ping" to ask if that person is there. When that person actually shows
  up and sees the ping, he will send you a message back saying "pong". Cute,
  isn't it? But hey, it works.

* Even if someone does end up answering your questions, it can happen that that
  person gets distracted by some other task and forget he/she was talking to
  you. Please don't take that as a sign we don't care about your questions. We
  do, but we too get busy sometimes: we're only human. If you were talking to
  somebody and haven't gotten any reply to your last message for some time, feel
  free to just ask again.

* If you've decided to pick up a good first bug, you can (in theory at least)
  get someone from the SDK team to mentor you. A mentor is someone who is
  already familiar with the code who can walk you through it, and who is your go
  to guy in case you have any questions about it. The idea of mentoring was
  introduced a while ago to make it easier for new contributors to familiarize
  themselves with the code. Unfortunately, it hasn't really caught on yet, but
  we're trying to change that. So by all means: ask!
