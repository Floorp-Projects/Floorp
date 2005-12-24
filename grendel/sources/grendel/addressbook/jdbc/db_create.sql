create table "AddressBookIndex"
(
        "id" INTEGER not null,
        "name" VARCHAR(200) not null,
        "email" VARCHAR(200) not null,
        primary key ("id")
)

create table "AddressList"
(
	"ID" INTEGER not null,
	"memberID" INTEGER not null
)

create table "AddressListIndex"
(
	"id" INTEGER not null,
	"name" VARCHAR(100) not null
)

create table "Addressbook"
(
	"id" INTEGER not null
,
	"feild" VARCHAR(50) not null,
	"value" VARCHAR(200) not null,
	"flags" VARCHAR(10) not null
)

INSERT INTO "Addressbook" VALUES (0,'cn','Grendel Development','')
INSERT INTO "Addressbook" VALUES (0,'name','Grendel Development','')
INSERT INTO "Addressbook" VALUES (0,'mail','grendel@mozilla.org','')
